# Penjelasan Perhitungan Fuzzy Tahani
## Sistem Pertanian Cerdas Greenhouse Cabai Rawit — ESP32 FreeRTOS

**Contoh data yang digunakan:** Suhu = 25.4°C, Kelembaban Tanah = 64%, pH = 5.45

---

## Apa itu Fuzzy?

Bayangkan kamu tanya ke seseorang: *"Apakah suhu 25.4°C itu panas?"*

Orang itu mungkin menjawab: *"Lumayan panas, tapi belum terlalu panas."*

Fuzzy bekerja seperti itu — **tidak hitam-putih (ya/tidak), tapi ada tingkatan** dari 0 sampai 1:
- `0` = tidak sama sekali masuk kategori ini
- `1` = sepenuhnya masuk kategori ini
- `0.5` = setengah-setengah

---

## Urutan Kerja Sistem di ESP32 (Core 1)

Setiap 15 detik, `taskSensor` menjalankan urutan ini:

```
1. Baca DHT22       → suhu udara + kelembaban udara
        ↓
2. Baca Sensor pH   → DMS ON 7 detik → 150 sampel ADC → DMS OFF
   (Kalman Filter + drift-guard + kompensasi suhu)
        ↓
3. Jeda Galvanik    → 4 detik (biarkan muatan elektroda pH terurai)
        ↓
4. Baca Soil Moisture → 50 sampel ADC → Kalman Filter
        ↓
5. inferensiFuzzy() → 3 rule → fire strength → keputusan relay
        ↓
6. terapkanAktuator() → kipas langsung, pompa via tickPompa()
        ↓
7. flagDataBaru = true → taskKomunikasi publish MQTT
```

> **Catatan penting:** Di versi ini, **pH dibaca SEBELUM soil**, bukan sebaliknya.
> Ini karena DMS (saklar daya elektroda) meninggalkan sisa potensial listrik
> yang bisa mengacaukan pembacaan ADC soil jika soil dibaca setelahnya tanpa jeda.

---

## Langkah 1 — Fuzzifikasi (Menghitung Derajat Keanggotaan)

### A. Suhu Udara (25.4°C)

Sistem membagi suhu ke 3 himpunan:

| Himpunan | Bentuk Kurva | Zona Penuh | Zona Transisi |
|----------|-------------|------------|---------------|
| Rendah | Trapesium kiri (bahu kiri) | ≤ 24°C | 24–27°C menurun |
| Sedang | Segitiga | Puncak di 27°C | 24–27°C naik, 27–31°C turun |
| Tinggi | Trapesium kanan (bahu kanan) | ≥ 31°C | 27–31°C naik |

**Perhitungan untuk x = 25.4°C:**

```
μ_Rendah = (27 - 25.4) / (27 - 24) = 1.6 / 3 = 0.533
```
Artinya: suhu 25.4°C masih **53.3% termasuk Rendah**

```
μ_Sedang = (25.4 - 24) / (27 - 24) = 1.4 / 3 = 0.467
```
Artinya: suhu 25.4°C **46.7% termasuk Sedang**

```
μ_Tinggi = 0  (karena 25.4 ≤ 27, belum memasuki zona Tinggi)
```

**Status Suhu = argmax(0.533, 0.467, 0.000) → "Rendah"**

---

### B. Kelembaban Tanah (64%)

| Himpunan | Bentuk Kurva | Zona Penuh | Zona Transisi |
|----------|-------------|------------|---------------|
| Kering | Trapesium kiri | ≤ 40% | 40–50% menurun |
| Lembab | Trapesium tengah | 50–70% | 40–50% naik, 70–80% turun |
| Basah | Trapesium kanan | ≥ 80% | 70–80% naik |

**Perhitungan untuk x = 64%:**

```
μ_Kering = 0    (64 ≥ 50, tanah tidak kering)
μ_Lembab = 1.0  (64 berada di zona plató 50–70%, sepenuhnya Lembab)
μ_Basah  = 0    (64 ≤ 70, belum masuk zona Basah)
```

**Status Tanah = argmax(0.000, 1.000, 0.000) → "Lembab"**

---

### C. pH Tanah (5.45)

| Himpunan | Bentuk Kurva | Zona Penuh | Zona Transisi |
|----------|-------------|------------|---------------|
| Asam | Trapesium kiri | ≤ 5.0 | 5.0–6.0 menurun |
| Normal | Trapesium tengah | 6.0–7.0 | 5.5–6.0 naik, 7.0–7.5 turun |
| Basa | Trapesium kanan | ≥ 7.5 | 7.0–7.5 naik |

**Perhitungan untuk x = 5.45:**

```
μ_Asam   = (6 - 5.45) / (6 - 5) = 0.55 / 1 = 0.55
```

```
μ_Normal = (5.45 - 5.5) / 0.5 = ...
         → 5.45 < 5.5, belum masuk zona Normal → 0.00
```

```
μ_Basa   = 0  (5.45 jauh dari 7.0)
```

**Status pH = argmax(0.55, 0.00, 0.00) → "Asam"**

---

## Langkah 2 — Inferensi: 3 Rule Fuzzy Tahani

Sistem menggunakan **3 rule independen**. Setiap rule dievaluasi secara
terpisah — bisa aktif bersamaan.

```cpp
FuzzyOutput inferensiFuzzy(float suhu, float soil, float pH) {
  float mu_st = fuzzyTempTinggi(suhu);  // fire strength R1
  float mu_tk = fuzzySoilKering(soil);  // fire strength R2
  float mu_pa = fuzzyPhAsam(pH);        // fire strength R3
  ...
}
```

### Rule 1 — Kipas Pendingin

```
IF suhu TINGGI → KIPAS ON

mu_st = fuzzyTempTinggi(25.4) = 0.000
out.mu_kipas = 0.000
out.kipas    = (0.000 > 0.5) → false → KIPAS OFF ❌
```

**Threshold kipas = 0.5** (lebih ketat karena kipas bekerja terus-menerus)

---

### Rule 2 — Pompa Air Irigasi

```
IF tanah KERING → POMPA AIR ON

mu_tk = fuzzySoilKering(64) = 0.000
out.mu_pompa_air = (soil > 0) ? mu_tk : 0  → 0.000
out.pompa_air    = (soil > 0 && 0.000 > 0.4) → false → POMPA AIR OFF ❌
```

Guard `soil > 0.0f` mencegah pompa menyala saat sensor tidak terpasang.

---

### Rule 3 — Pompa Koreksi pH

```
IF pH ASAM → POMPA pH ON

mu_pa = fuzzyPhAsam(5.45) = 0.55
out.mu_pompa_ph = (pH > 0) ? mu_pa : 0  → 0.55
out.pompa_ph    = (pH > 0 && 0.55 > 0.4) → true → POMPA pH ON ✅
```

Guard `pH > 0.0f` mencegah pompa menyala saat probe pH tidak terpasang
(sistem mengembalikan 0.0f saat probe lepas atau warmup belum selesai).

> **Catatan:** Di versi kode ini, Rule 3 hanya merespons pH **Asam**.
> pH Basa tidak mengaktifkan pompa — sesuai implementasi aktual di
> `inferensiFuzzy()` yang hanya menggunakan `fuzzyPhAsam()`.

---

## Langkah 3 — Keputusan Aktuator & Timer Pompa

Hasil inferensi tidak langsung menggerakkan relay pompa.
Sistem melewati mekanisme **tickPompa()** yang dipanggil setiap 20ms:

```
Jalur AUTO (dari fuzzy):
  Pompa Air: reqPompaAir = true
    → tunggu jeda habis → nyala 10 detik → jeda 30 menit
  Pompa pH:  reqPompaPH = true
    → tunggu jeda habis → nyala 10 detik → jeda 3 jam
  Kipas: langsung ON/OFF tanpa jeda

Jalur MANUAL (dari dashboard via MQTT):
  manualPompaAir / manualPompaPH = true
    → langsung nyala, tidak ada timer, tidak pakai jeda AUTO
    → flagRelayBerubah = true → Supabase diupdate segera (tidak tunggu 60 detik)
```

### Tabel Keputusan Akhir (contoh data)

| Aktuator | μ (Fire Strength) | Threshold | Keputusan | Timer |
|----------|------------------|-----------|-----------|-------|
| 🌀 Kipas | 0.000 | > 0.50 | **OFF** | — |
| 💦 Pompa Air | 0.000 | > 0.40 | **OFF** | — |
| 🧪 Pompa pH | 0.550 | > 0.40 | **ON** | 10 detik, jeda 3 jam |

---

## Penjelasan Kode di `index.ino`

### Fungsi Keanggotaan Suhu

```cpp
static float fuzzyTempRendah(float x) {
  if (x <= 24.0f) return 1.0f;             // ≤ 24°C → sepenuhnya Rendah
  if (x < 27.0f)  return (27.0f - x) / 3.0f;  // 24–27°C → menurun
  return 0.0f;                              // ≥ 27°C → bukan Rendah
}
```
Untuk x = 25.4: `(27.0 - 25.4) / 3.0 = 0.533`

```cpp
static float fuzzyTempSedang(float x) {
  if (x <= 24.0f || x >= 31.0f) return 0.0f;  // di luar 24–31 → 0
  if (x <= 27.0f) return (x - 24.0f) / 3.0f;  // 24–27 → naik ke 1
  return (31.0f - x) / 4.0f;                   // 27–31 → turun ke 0
}
```
Untuk x = 25.4: `(25.4 - 24.0) / 3.0 = 0.467`

```cpp
static float fuzzyTempTinggi(float x) {
  if (x <= 27.0f) return 0.0f;             // ≤ 27°C → belum Tinggi
  if (x < 31.0f)  return (x - 27.0f) / 4.0f;  // 27–31°C → naik ke 1
  return 1.0f;                              // ≥ 31°C → sepenuhnya Tinggi
}
```
Untuk x = 25.4: 25.4 ≤ 27 → **0.000**

---

### Fungsi Keanggotaan Kelembaban Tanah

```cpp
static float fuzzySoilKering(float x) {
  if (x <= 40.0f) return 1.0f;             // ≤ 40% → sepenuhnya Kering
  if (x < 50.0f)  return (50.0f - x) / 10.0f; // 40–50% → menurun
  return 0.0f;                              // ≥ 50% → bukan Kering
}
```
Untuk x = 64: 64 ≥ 50 → **0.000**

```cpp
static float fuzzySoilLembab(float x) {
  if (x <= 40.0f || x >= 80.0f) return 0.0f;  // di luar 40–80 → 0
  if (x < 50.0f)  return (x - 40.0f) / 10.0f; // 40–50 → naik ke 1
  if (x <= 70.0f) return 1.0f;                  // 50–70 → zona penuh Lembab
  return (80.0f - x) / 10.0f;                   // 70–80 → turun ke 0
}
```
Untuk x = 64: berada di zona 50–70 → **1.000**

```cpp
static float fuzzySoilBasah(float x) {
  if (x <= 70.0f) return 0.0f;             // ≤ 70% → belum Basah
  if (x < 80.0f)  return (x - 70.0f) / 10.0f; // 70–80% → naik ke 1
  return 1.0f;                              // ≥ 80% → sepenuhnya Basah
}
```
Untuk x = 64: 64 ≤ 70 → **0.000**

---

### Fungsi Keanggotaan pH

```cpp
static float fuzzyPhAsam(float x) {
  if (x <= 5.0f) return 1.0f;          // ≤ 5 → sepenuhnya Asam
  if (x < 6.0f)  return (6.0f - x);   // 5–6 → menurun (pembagi = 1, jadi langsung selisih)
  return 0.0f;                          // ≥ 6 → bukan Asam
}
```
Untuk x = 5.45: `(6.0 - 5.45) = 0.55`

```cpp
static float fuzzyPhNormal(float x) {
  if (x <= 5.5f || x >= 7.5f) return 0.0f;  // di luar 5.5–7.5 → 0
  if (x < 6.0f)  return (x - 5.5f) / 0.5f;  // 5.5–6.0 → naik ke 1
  if (x <= 7.0f) return 1.0f;                 // 6.0–7.0 → zona penuh Normal
  return (7.5f - x) / 0.5f;                   // 7.0–7.5 → turun ke 0
}
```
Untuk x = 5.45: 5.45 < 5.5 → **0.000**

```cpp
static float fuzzyPhBasa(float x) {
  if (x <= 7.0f) return 0.0f;           // ≤ 7 → belum Basa
  if (x < 7.5f)  return (x - 7.0f) / 0.5f; // 7.0–7.5 → naik ke 1
  return 1.0f;                            // ≥ 7.5 → sepenuhnya Basa
}
```
Untuk x = 5.45: 5.45 ≤ 7.0 → **0.000**

---

### Fungsi Status (Argmax)

```cpp
static const char* fuzzyTempStatus(float x) {
  float r = fuzzyTempRendah(x);   // = 0.533
  float s = fuzzyTempSedang(x);   // = 0.467
  float t = fuzzyTempTinggi(x);   // = 0.000
  if (r >= s && r >= t) return "Rendah";  // r terbesar → Rendah
  if (s >= t)           return "Sedang";
  return "Tinggi";
}
```
Untuk x = 25.4°C: r=0.533 terbesar → kembalikan **"Rendah"**

Fungsi serupa berlaku untuk `fuzzySoilStatus()` dan `fuzzyPhStatus()`.

---

### Fungsi Inferensi — Inti Sistem

```cpp
FuzzyOutput inferensiFuzzy(float suhu, float soil, float pH) {
  FuzzyOutput out;

  // Hitung fire strength untuk setiap rule
  float mu_st = fuzzyTempTinggi(suhu);   // = 0.000 untuk suhu 25.4°C
  float mu_tk = fuzzySoilKering(soil);   // = 0.000 untuk soil 64%
  float mu_pa = fuzzyPhAsam(pH);         // = 0.550 untuk pH 5.45

  // Status himpunan dominan (ditampilkan di dashboard)
  out.status_suhu  = fuzzyTempStatus(suhu);   // "Rendah"
  out.status_tanah = fuzzySoilStatus(soil);   // "Lembab"
  out.status_ph    = fuzzyPhStatus(pH);       // "Asam"

  // R1: suhu Tinggi → Kipas ON
  out.mu_kipas = mu_st;                  // = 0.000
  out.kipas    = (mu_st > 0.5f);         // 0.000 > 0.5 → false

  // R2: tanah Kering → Pompa Air ON
  // Guard: soil <= 0 berarti sensor tidak terpasang → paksa 0
  out.mu_pompa_air = (soil <= 0.0f) ? 0.0f : mu_tk;  // = 0.000
  out.pompa_air    = (soil > 0.0f && out.mu_pompa_air > 0.4f); // false

  // R3: pH Asam → Pompa pH ON
  // Guard: pH <= 0 berarti probe lepas / warmup → paksa 0
  out.mu_pompa_ph = (pH <= 0.0f) ? 0.0f : mu_pa;   // = 0.550
  out.pompa_ph    = (pH > 0.0f && mu_pa > 0.4f);   // true ✅

  return out;
}
```

**Perbedaan penting dari versi sebelumnya:**
- Hanya 3 rule murni, tidak ada rule gabungan AND/OR
- Guard `soil <= 0` dan `pH <= 0` untuk proteksi sensor tidak terpasang
- Tidak ada `fuzzyPhBasa()` dalam rule — hanya `fuzzyPhAsam()` yang memicu pompa pH

---

### Mekanisme tickPompa() — Anti Over-Watering

```cpp
void tickPompa() {
  // Dipanggil setiap 20ms dari taskSensor loop

  if (manualPompaAir) {
    // MANUAL: nyala terus, tidak ada timer durasi
    setRelay(RELAY_POMPA_AIR, true);
    flagRelayBerubah = true;  // ← BARU: Supabase diupdate segera
  } else if (reqPompaAir) {
    if (jeda_habis) {
      setRelay(RELAY_POMPA_AIR, true);
      catat waktu nyala;
      flagRelayBerubah = true;
    }
  } else if (sedang_nyala_AUTO) {
    if (sudah 10 detik) {
      setRelay(RELAY_POMPA_AIR, false);
      mulai_jeda_30_menit;
      flagRelayBerubah = true;
    }
  }
}
```

`flagRelayBerubah` adalah fitur baru — saat relay berubah state, Supabase
langsung diupdate tanpa menunggu interval 60 detik normal.

---

## Ringkasan Alur Lengkap (Contoh Data)

```
INPUT SENSOR
  Suhu = 25.4°C  |  Soil = 64%  |  pH = 5.45
         ↓
FUZZIFIKASI
  Suhu:  μ_Rendah = 0.533  μ_Sedang = 0.467  μ_Tinggi = 0.000  → "Rendah"
  Tanah: μ_Kering = 0.000  μ_Lembab = 1.000  μ_Basah  = 0.000  → "Lembab"
  pH:    μ_Asam   = 0.550  μ_Normal = 0.000  μ_Basa   = 0.000  → "Asam"
         ↓
INFERENSI (3 rule independen)
  R1: μ_Kipas    = μ_Tinggi(25.4) = 0.000
  R2: μ_PompaAir = μ_Kering(64)   = 0.000
  R3: μ_PompaPH  = μ_Asam(5.45)   = 0.550
         ↓
KEPUTUSAN AKTUATOR (threshold)
  R1: 0.000 > 0.50 ? → TIDAK  → 🌀 Kipas     OFF ❌
  R2: 0.000 > 0.40 ? → TIDAK  → 💦 Pompa Air OFF ❌
  R3: 0.550 > 0.40 ? → YA     → 🧪 Pompa pH  ON  ✅
         ↓
EKSEKUSI (via tickPompa)
  Pompa pH: reqPompaPH = true
    → cek jeda 3 jam
    → jika boleh: nyala 10 detik → OFF → jeda 3 jam
    → flagRelayBerubah = true → Supabase update segera
         ↓
OUTPUT AKHIR
  🌀 Kipas Pendingin  → MATI
  💦 Pompa Irigasi    → MATI
  🧪 Pompa Koreksi pH → HIDUP selama 10 detik
                        (memasukkan larutan dolomit/pH-up agar pH naik ke 6–7)
```

---

## Apa yang Ditampilkan di Dashboard?

Setelah `inferensiFuzzy()` selesai, data dikirim via MQTT ke dashboard web:

| Field MQTT | Nilai | Tampilan di Dashboard |
|-----------|-------|----------------------|
| `status_suhu` | "Rendah" | Badge kuning "Rendah" |
| `status_tanah` | "Lembab" | Badge hijau "Lembab" |
| `status_ph` | "Asam" | Badge kuning "Asam" |
| `fuzzy_suhu` | 0.000 | Bar biru 0% |
| `fuzzy_soil` | 0.000 | Bar hijau 0% |
| `fuzzy_ph` | 0.550 | Bar oranye 55% |
| `relay_kipas` | 0.0 | Indikator OFF (abu) |
| `relay_air` | 0.0 | Indikator OFF (abu) |
| `relay_ph` | 1.0 | Indikator **ON (hijau)** |
| `countdown_ph` | -1 | Tersembunyi (pompa sedang nyala) |

---

*File ini menjelaskan implementasi Fuzzy Tahani pada sistem pertanian cerdas
greenhouse cabai rawit berbasis ESP32 FreeRTOS — disesuaikan dengan kode aktual
`index/index.ino` versi terbaru (urutan baca pH → soil, 3 rule murni,
flagRelayBerubah untuk update Supabase segera saat relay berubah).*
