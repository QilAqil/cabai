# Metode Fuzzy Tahani: Mengubah Data Sensor Menjadi Keputusan Otomatis
## Sistem Pertanian Cerdas Cabai Rawit — Penjelasan Lengkap

---

## Gambaran Umum

Metode Fuzzy Tahani bekerja sebagai **jembatan** antara data sensor
(angka mentah) dan aktuator (kipas, pompa air, pompa pH).

Tanpa fuzzy, sistem hanya bisa bertanya:
> *"Apakah suhu sudah tinggi?"* → Ya atau Tidak

Dengan Fuzzy Tahani, sistem bertanya:
> *"Seberapa tinggi suhu ini?"* → 0% sampai 100%

Lalu membuat keputusan berdasarkan jawabannya secara bertahap dan gradual —
lebih sesuai dengan cara petani berpikir di lapangan nyata.

---

## Alur Kerja Keseluruhan

```
DATA SENSOR  →  FUZZIFIKASI  →  EVALUASI RULE  →  KEPUTUSAN AKTUATOR
(angka crisp)    (angka → derajat)   (fire strength)    (ON / OFF)
```

Proses ini terdiri dari **4 tahap berurutan** yang dijalankan otomatis
oleh ESP32 setiap 15 detik.

---

## TAHAP 1 — Input: Data Sensor (Crisp Value)

Sensor membaca nilai nyata dari lingkungan greenhouse setiap 15 detik:

| Sensor | GPIO | Contoh Nilai | Satuan |
|--------|------|-------------|--------|
| DHT22 | GPIO 4 | Suhu = **30.0** | °C |
| Soil Moisture | GPIO 35 | Kelembapan = **35** | % |
| Sensor pH + DMS | GPIO 34 | pH = **5.20** | pH |

Nilai ini disebut **crisp value** — angka tegas hasil pembacaan sensor.
Belum bisa langsung diputuskan "berbahaya" atau "aman" hanya dari angkanya.

---

## TAHAP 2 — Fuzzifikasi: Angka → Derajat Keanggotaan

Setiap crisp value dimasukkan ke **fungsi keanggotaan (membership function)**
untuk menghasilkan nilai antara 0 dan 1.

Nilai ini menunjukkan **seberapa besar** suatu kondisi termasuk dalam
suatu kategori (himpunan fuzzy).

```
0.0 = sama sekali tidak termasuk
0.5 = setengah termasuk
1.0 = sepenuhnya termasuk
```

---

### 2a. Fuzzifikasi Suhu Udara

Tiga himpunan fuzzy untuk suhu:

```
         1 ┤████████████╲              ╱╲ Tinggi
           │  Rendah      ╲  Sedang  ╱   ╲............
       0.5 ┤               ╲        ╱     ╲
           │                ╲      ╱       ╲
         0 ┼────────────────────────────────────────→ °C
           0             24  27         31          45
```

**Rumus fungsi keanggotaan:**

```
             1              jika suhu ≤ 24
μ_Rendah =   (27 − suhu)/3  jika 24 < suhu < 27
             0              jika suhu ≥ 27

             0              jika suhu ≤ 24 atau ≥ 31
μ_Sedang =   (suhu − 24)/3  jika 24 < suhu ≤ 27
             (31 − suhu)/4  jika 27 < suhu < 31

             0              jika suhu ≤ 27
μ_Tinggi =   (suhu − 27)/4  jika 27 < suhu < 31
             1              jika suhu ≥ 31
```

**Contoh — Suhu = 30°C:**
```
μ_Rendah(30) = 0           → 30 ≥ 27, pasti bukan Rendah
μ_Sedang(30) = (31−30)/4   = 1/4  = 0.25
μ_Tinggi(30) = (30−27)/4   = 3/4  = 0.75

Himpunan dominan → TINGGI (μ = 0.75)
Artinya: suhu 30°C sudah 75% masuk kategori TINGGI
```

---

### 2b. Fuzzifikasi Kelembapan Tanah

Tiga himpunan fuzzy untuk kelembapan tanah:

```
         1 ┤████████╲          Lembab████████╲  Basah
           │  Kering  ╲      ╱               ╲  ╱.....
       0.5 ┤            ╲  ╱                   ╲╱
           │              ╲╱
         0 ┼────────────────────────────────────────→ %
           0    40    50            70    80         100
```

**Rumus fungsi keanggotaan:**

```
             1               jika tanah ≤ 40
μ_Kering =   (50−tanah)/10   jika 40 < tanah < 50
             0               jika tanah ≥ 50

             0               jika tanah ≤ 40 atau ≥ 80
μ_Lembab =   (tanah−40)/10   jika 40 < tanah < 50
             1               jika 50 ≤ tanah ≤ 70
             (80−tanah)/10   jika 70 < tanah < 80

             0               jika tanah ≤ 70
μ_Basah  =   (tanah−70)/10   jika 70 < tanah < 80
             1               jika tanah ≥ 80
```

**Contoh — Tanah = 35%:**
```
μ_Kering(35) = 1.00   → 35 ≤ 40, sepenuhnya KERING
μ_Lembab(35) = 0.00
μ_Basah (35) = 0.00

Himpunan dominan → KERING (μ = 1.00)
Artinya: tanah 35% SEPENUHNYA kering — darurat irigasi
```

---

### 2c. Fuzzifikasi pH Tanah

Tiga himpunan fuzzy untuk pH tanah:

```
         1 ┤███████╲         Normal████████╲      ╱ Basa
           │  Asam   ╲     ╱               ╲    ╱
       0.5 ┤           ╲ ╱                   ╲╱.......
           │             ╲╱╲
         0 ┼────────────────────────────────────────→ pH
           3    5  5.5  6              7  7.5         9
```

**Rumus fungsi keanggotaan:**

```
             1             jika pH ≤ 5.0
μ_Asam   =   (6−pH)/1      jika 5.0 < pH < 6.0
             0             jika pH ≥ 6.0

             0             jika pH ≤ 5.5 atau ≥ 7.5
μ_Normal =   (pH−5.5)/0.5  jika 5.5 < pH < 6.0
             1             jika 6.0 ≤ pH ≤ 7.0
             (7.5−pH)/0.5  jika 7.0 < pH < 7.5

             0             jika pH ≤ 7.0
μ_Basa   =   (pH−7)/0.5    jika 7.0 < pH < 7.5
             1             jika pH ≥ 7.5
```

**Contoh — pH = 5.20:**
```
μ_Asam  (5.20) = (6−5.20)/1   = 0.80/1  = 0.80
μ_Normal(5.20) = (5.20−5.5)/0.5 → negatif = 0.00
μ_Basa  (5.20) = 0.00

Himpunan dominan → ASAM (μ = 0.80)
Artinya: pH 5.20 sudah 80% termasuk kategori ASAM
```

---

### Ringkasan Hasil Fuzzifikasi (Contoh Input: 30°C / 35% / pH 5.20)

| Variabel | Nilai | Himpunan | μ Hasil |
|----------|-------|----------|---------|
| Suhu | 30°C | Tinggi | **0.75** |
| Tanah | 35% | Kering | **1.00** |
| pH | 5.20 | Asam | **0.80** |

---

## TAHAP 3 — Evaluasi Rule: Menghitung Fire Strength

Derajat keanggotaan dimasukkan ke dalam **3 rule IF-THEN**:

### Rule 1 — Kipas Pendingin
```
IF  suhu udara adalah TINGGI
THEN  kipas pendingin ON

Fire Strength R1 = μ_Tinggi(suhu) = 0.75
```

### Rule 2 — Pompa Air Irigasi
```
IF  kelembapan tanah adalah KERING
THEN  pompa air ON

Fire Strength R2 = μ_Kering(tanah) = 1.00
```

### Rule 3 — Pompa Koreksi pH
```
IF  pH tanah adalah ASAM  ATAU  pH tanah adalah BASA
THEN  pompa koreksi pH ON

Fire Strength R3 = MAX(μ_Asam, μ_Basa)
                 = MAX(0.80, 0.00)
                 = 0.80
```

> **Mengapa Rule 3 menggunakan MAX (OR)?**
> Karena pH asam DAN pH basa sama-sama berbahaya bagi tanaman cabai rawit.
> Jika salah satu terjadi, pompa pH harus menyala.
> Operator OR mengambil nilai terbesar dari keduanya.

---

## TAHAP 4 — Keputusan Aktuator

Fire strength dibandingkan dengan **threshold** yang telah ditetapkan:

| Rule | Fire Strength | Threshold | Perbandingan | Keputusan |
|------|--------------|-----------|-------------|-----------|
| R1 Kipas | 0.75 | > 0.50 | 0.75 > 0.50 ✓ | ✅ **Kipas ON** |
| R2 Pompa Air | 1.00 | > 0.40 | 1.00 > 0.40 ✓ | ✅ **Pompa Air ON** |
| R3 Pompa pH | 0.80 | > 0.40 | 0.80 > 0.40 ✓ | ✅ **Pompa pH ON** |

---

## Diagram Alur Lengkap (Contoh: 30°C / 35% / pH 5.20)

```
┌───────────────────────────────────────────────────────────────────┐
│  TAHAP 1 — INPUT SENSOR                                            │
│  Suhu = 30°C   │   Tanah = 35%   │   pH = 5.20                    │
└────────┬──────────────┬───────────────────┬───────────────────────┘
         │              │                   │
         ▼              ▼                   ▼
┌───────────────────────────────────────────────────────────────────┐
│  TAHAP 2 — FUZZIFIKASI                                             │
│  μ_Tinggi = 0.75  │  μ_Kering = 1.00  │  μ_Asam = 0.80          │
└────────┬──────────────┬───────────────────┬───────────────────────┘
         │              │                   │
         ▼              ▼                   ▼
┌───────────────────────────────────────────────────────────────────┐
│  TAHAP 3 — EVALUASI RULE (Fire Strength)                           │
│  R1 = 0.75        │  R2 = 1.00        │  R3 = max(0.80,0)=0.80   │
└────────┬──────────────┬───────────────────┬───────────────────────┘
         │              │                   │
         ▼              ▼                   ▼
┌───────────────────────────────────────────────────────────────────┐
│  TAHAP 4 — KEPUTUSAN AKTUATOR                                      │
│                                                                    │
│  0.75 > 0.50 → 🌀 KIPAS ON                                        │
│  1.00 > 0.40 → 💦 POMPA AIR ON   (nyala 10 dtk, jeda 30 mnt)     │
│  0.80 > 0.40 → 🧪 POMPA pH  ON   (nyala 10 dtk, jeda 3 jam)      │
└───────────────────────────────────────────────────────────────────┘
```

---

## Perbandingan: Fuzzy Tahani vs Logika Biasa (IF-ELSE)

### Cara logika biasa bekerja:
```python
if suhu > 31:
    kipas = ON
else:
    kipas = OFF
```

**Masalah:** Suhu 30.9°C → kipas OFF. Suhu 31.1°C → kipas ON.
Selisih 0.2°C menghasilkan keputusan yang **berubah drastis**.

---

### Cara Fuzzy Tahani bekerja:
```
Suhu 28°C → μ_Tinggi = 0.25 → 0.25 > 0.50? TIDAK → Kipas OFF
Suhu 30°C → μ_Tinggi = 0.75 → 0.75 > 0.50? YA   → Kipas ON
Suhu 31°C → μ_Tinggi = 1.00 → 1.00 > 0.50? YA   → Kipas ON
```

Keputusan **bersifat gradual** — suhu 30°C sudah dianggap 75% "tinggi",
jauh sebelum mencapai 31°C.

---

### Tabel Perbandingan Lengkap

| Suhu | Logika Biasa | μ_Tinggi | Fuzzy Tahani |
|------|-------------|----------|-------------|
| 27°C | OFF | 0.00 | OFF |
| 28°C | OFF | 0.25 | OFF (0.25 < 0.50) |
| 29°C | OFF | 0.50 | OFF (0.50 tidak > 0.50) |
| 29.1°C | OFF | 0.525 | **ON** (0.525 > 0.50) |
| 30°C | OFF | 0.75 | **ON** |
| 31°C | **ON** | 1.00 | **ON** |
| 32°C | **ON** | 1.00 | **ON** |

> **Kesimpulan:** Fuzzy Tahani mulai mengaktifkan kipas pada **suhu ~29.1°C**
> (saat μ baru melewati 0.50), bukan tiba-tiba di 31°C.
> Ini lebih sesuai dengan kondisi pertanian nyata.

---

## Kasus Khusus: Saat Sensor Tidak Aktif

Sistem juga memiliki **perlindungan** jika sensor belum terpasang atau
sedang dalam fase warmup:

```
Jika pH = 0.0 (sensor belum siap):
  μ_Pompa pH = 0.0  →  Pompa pH tetap OFF
  (tidak salah aktifkan pompa saat data tidak valid)

Jika Soil = 0.0 (sensor dicabut):
  μ_Pompa Air = 0.0  →  Pompa Air tetap OFF
```

---

## Implementasi dalam Kode ESP32

Berikut fungsi inferensi Fuzzy Tahani yang dijalankan di Core 1:

```cpp
FuzzyOutput inferensiFuzzy(float suhu, float soil, float pH) {
    // Hitung derajat keanggotaan
    float mu_st = fuzzyTempTinggi(suhu);   // R1
    float mu_tk = fuzzySoilKering(soil);   // R2
    float mu_pa = fuzzyPhAsam(pH);         // R3 bagian asam
    float mu_pb = fuzzyPhBasa(pH);         // R3 bagian basa (tidak tampil di sini)

    // R1: Suhu Tinggi → Kipas ON
    out.mu_kipas = mu_st;
    out.kipas    = (mu_st > 0.5f);

    // R2: Tanah Kering → Pompa Air ON
    out.mu_pompa_air = (soil <= 0.0f) ? 0.0f : mu_tk;
    out.pompa_air    = (soil > 0.0f && out.mu_pompa_air > 0.4f);

    // R3: pH Asam atau Basa → Pompa pH ON
    out.mu_pompa_ph = (pH <= 0.0f) ? 0.0f : mu_pa;
    out.pompa_ph    = (pH > 0.0f && mu_pa > 0.4f);
}
```

Fungsi ini dipanggil setiap 15 detik dari `taskSensor` di **Core 1 ESP32**.
Hasilnya dikirim ke `taskKomunikasi` di Core 0 via MQTT untuk ditampilkan
di dashboard web secara real-time.

---

## Ringkasan dalam Satu Tabel

| Tahap | Proses | Input | Output |
|-------|--------|-------|--------|
| 1 | Baca sensor | — | Suhu, Tanah%, pH (angka crisp) |
| 2 | Fuzzifikasi | Angka crisp | Derajat keanggotaan μ (0–1) |
| 3 | Evaluasi rule | Nilai μ | Fire strength per aktuator |
| 4 | Keputusan | Fire strength vs threshold | ON / OFF aktuator |

---

## Contoh Tambahan: Kondisi Normal (Semua OFF)

**Input:** Suhu = 24.4°C, Tanah = 49%, pH = 5.95

```
Fuzzifikasi:
  μ_Tinggi(24.4) = 0.00  → suhu belum tinggi
  μ_Kering(49)   = 0.10  → tanah sedikit mulai mengering
  μ_Asam  (5.95) = 0.05  → pH sedikit mendekati asam

Fire Strength:
  R1 = 0.00
  R2 = 0.10
  R3 = max(0.05, 0.00) = 0.05

Keputusan:
  0.00 > 0.50? TIDAK → Kipas     OFF ✗
  0.10 > 0.40? TIDAK → Pompa Air OFF ✗
  0.05 > 0.40? TIDAK → Pompa pH  OFF ✗
```

> Semua aktuator OFF — kondisi mendekati optimal.
> Sistem tidak melakukan intervensi yang tidak perlu.
> Ini membuktikan sistem **tidak boros energi** saat tanaman dalam kondisi baik.

---

*Dokumen ini menjelaskan alur kerja Fuzzy Tahani dari data sensor
hingga keputusan aktuator pada sistem pertanian cerdas cabai rawit.*
*Disesuaikan dengan implementasi aktual di `index/index.ino`.*
