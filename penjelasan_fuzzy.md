# Penjelasan Perhitungan Fuzzy Tahani
## Sistem Monitoring Greenhouse Cabai Rawit

**Contoh data yang digunakan:** Suhu = 25.4°C, Kelembaban Tanah = 64%, pH = 5.45

---

## Apa itu Fuzzy?

Bayangkan kamu tanya ke seseorang: *"Apakah suhu 25.4°C itu panas?"*

Orang itu mungkin menjawab: *"Lumayan panas, tapi belum terlalu panas."*

Fuzzy bekerja seperti itu — **tidak hitam-putih (ya/tidak), tapi ada tingkatan** dari 0 sampai 1:
- `0` = tidak sama sekali
- `1` = sepenuhnya
- `0.5` = setengah-setengah

---

## Langkah 1 — Fuzzifikasi (Menghitung Derajat Keanggotaan)

### A. Suhu Udara (25.4°C)

Sistem membagi suhu ke 3 himpunan:

| Himpunan | Bentuk | Rentang |
|---|---|---|
| Rendah | Trapesium kiri | ≤ 24°C penuh, 24–27°C menurun |
| Sedang | Segitiga | 24–31°C |
| Tinggi | Trapesium kanan | ≥ 31°C penuh, 27–31°C naik |

**Perhitungan:**

```
μ_Rendah = (27 - 25.4) / (27 - 24) = 1.6 / 3 = 0.533
```
Artinya: suhu ini masih **53.3% termasuk Rendah**

```
μ_Sedang = (25.4 - 24) / (27 - 24) = 1.4 / 3 = 0.467
```
Artinya: suhu ini **46.7% termasuk Sedang**

```
μ_Tinggi = 0  (suhu 25.4 ≤ 27, belum masuk Tinggi)
```

**Status Suhu → ambil nilai terbesar → "Rendah" (0.533)**

---

### B. Kelembaban Tanah (64%)

Sistem membagi kelembaban tanah ke 3 himpunan:

| Himpunan | Bentuk | Rentang |
|---|---|---|
| Kering | Trapesium kiri | ≤ 40% penuh, 40–50% menurun |
| Lembab | Trapesium tengah | 40–80% |
| Basah | Trapesium kanan | ≥ 80% penuh, 70–80% naik |

**Perhitungan:**

```
μ_Kering = 0  (64 ≥ 50, tanah tidak kering)
μ_Lembab = 1.0  (64 berada di zona 50–70, penuh Lembab)
μ_Basah  = 0  (64 ≤ 70, tanah belum basah)
```

**Status Tanah → ambil nilai terbesar → "Lembab" (1.0)**

---

### C. pH Tanah (5.45)

Sistem membagi pH ke 3 himpunan:

| Himpunan | Bentuk | Rentang |
|---|---|---|
| Asam | Trapesium kiri | ≤ 5 penuh, 5–6 menurun |
| Normal | Trapesium tengah | 5.5–7.5 |
| Basa | Trapesium kanan | ≥ 7.5 penuh, 7–7.5 naik |

**Perhitungan:**

```
μ_Asam   = (6 - 5.45) / (6 - 5) = 0.55 / 1 = 0.55
μ_Normal = 0  (5.45 ≤ 5.5, belum masuk Normal)
μ_Basa   = 0  (5.45 jauh dari 7)
```

**Status pH → ambil nilai terbesar → "Asam" (0.55)**

---

## Langkah 2 — Inferensi (Evaluasi Rule)

Sistem punya 5 aturan. Operator yang digunakan:
- **AND** = ambil nilai **terkecil** (keduanya harus terpenuhi)
- **OR** = ambil nilai **terbesar** (salah satu cukup)

### Rule 1 — Kipas Pendingin
```
IF suhu TINGGI → KIPAS ON

μ_Tinggi = 0
Kipas = 0 → 0 ≤ 0.5 → KIPAS OFF ❌
```

### Rule 2 — Pompa Air (kondisi kering)
```
IF tanah KERING → POMPA AIR ON

μ_Kering = 0  → tidak memenuhi syarat
```

### Rule 3 — Pompa Air (kondisi lembab + panas)
```
IF tanah LEMBAB AND suhu TINGGI → POMPA AIR ON

AND = min(μ_Lembab, μ_Tinggi) = min(1.0, 0) = 0
```
Kenapa ambil terkecil? Karena syarat AND harus **keduanya terpenuhi**. Kalau satu saja nol, hasilnya nol.

**Gabung Rule 2 dan 3 dengan OR:**
```
OR = max(0, 0) = 0
Pompa Air = 0 → 0 ≤ 0.4 → POMPA AIR OFF ❌
```

### Rule 4 — Pompa Koreksi pH
```
IF pH ASAM → POMPA pH ON

μ_Asam = 0.55
Pompa pH = 0.55 → 0.55 > 0.4 → POMPA pH ON ✅
```

---

## Langkah 3 — Defuzzifikasi (Keputusan Akhir)

Sistem menggunakan **metode threshold** — output langsung berupa keputusan ON/OFF:

| Aktuator | Nilai μ | Threshold | Keputusan |
|---|---|---|---|
| 🌀 Kipas Pendingin | 0.00 | μ > 0.5 | **OFF** |
| 💦 Pompa Irigasi | 0.00 | μ > 0.4 | **OFF** |
| 🧪 Pompa Koreksi pH | 0.55 | μ > 0.4 | **ON** |

---

## Penjelasan Kode di index.ino

### Fungsi Keanggotaan Suhu

```cpp
static float fuzzyTempRendah(float x) {
  if (x <= 24.0f)                    return 1.0f;
  else if (x > 24.0f && x < 27.0f)  return (27.0f - x) / 3.0f;
  else                               return 0.0f;
}
```
- Baris 1: Jika suhu ≤ 24 → kembalikan 1.0 (sepenuhnya Rendah)
- Baris 2: Jika suhu 24–27 → hitung `(27 - x) / 3` (makin dekat 27, makin kecil)
- Baris 3: Jika suhu ≥ 27 → kembalikan 0 (bukan Rendah)

Untuk x = 25.4: `(27.0 - 25.4) / 3.0 = 0.533`

---

```cpp
static float fuzzyTempSedang(float x) {
  if (x <= 24.0f || x >= 31.0f)     return 0.0f;
  else if (x > 24.0f && x <= 27.0f) return (x - 24.0f) / 3.0f;
  else                               return (31.0f - x) / 4.0f;
}
```
- Baris 1: Di luar 24–31 → 0
- Baris 2: Suhu 24–27 → naik dari 0 ke 1
- Baris 3: Suhu 27–31 → turun dari 1 ke 0

Untuk x = 25.4: `(25.4 - 24.0) / 3.0 = 0.467`

---

```cpp
static float fuzzyTempTinggi(float x) {
  if (x <= 27.0f)                    return 0.0f;
  else if (x > 27.0f && x < 31.0f)  return (x - 27.0f) / 4.0f;
  else                               return 1.0f;
}
```
- Baris 1: Suhu ≤ 27 → 0 (belum Tinggi)
- Baris 2: Suhu 27–31 → naik dari 0 ke 1
- Baris 3: Suhu ≥ 31 → 1.0 (sepenuhnya Tinggi)

Untuk x = 25.4: 25.4 ≤ 27 → **0**

---

### Fungsi Keanggotaan Kelembaban Tanah

```cpp
static float fuzzySoilLembab(float x) {
  if (x <= 40.0f || x >= 80.0f)      return 0.0f;
  else if (x > 40.0f && x < 50.0f)   return (x - 40.0f) / 10.0f;
  else if (x >= 50.0f && x <= 70.0f) return 1.0f;
  else                                return (80.0f - x) / 10.0f;
}
```
- Baris 1: Di luar 40–80 → 0
- Baris 2: Soil 40–50 → naik dari 0 ke 1
- Baris 3: Soil 50–70 → 1.0 (zona penuh Lembab)
- Baris 4: Soil 70–80 → turun dari 1 ke 0

Untuk x = 64: berada di 50–70 → **1.0**

---

### Fungsi Keanggotaan pH

```cpp
static float fuzzyPhAsam(float x) {
  if (x <= 5.0f)                   return 1.0f;
  else if (x > 5.0f && x < 6.0f)  return (6.0f - x) / 1.0f;
  else                             return 0.0f;
}
```
- Baris 1: pH ≤ 5 → 1.0 (sepenuhnya Asam)
- Baris 2: pH 5–6 → menurun dari 1 ke 0
- Baris 3: pH ≥ 6 → 0 (bukan Asam)

Untuk x = 5.45: `(6.0 - 5.45) / 1.0 = 0.55`

---

### Fungsi Status (Argmax)

```cpp
static const char* fuzzyTempStatus(float x) {
  float r = fuzzyTempRendah(x);   // = 0.533
  float s = fuzzyTempSedang(x);   // = 0.467
  float t = fuzzyTempTinggi(x);   // = 0.000
  if (r >= s && r >= t) return "Rendah";
  else if (s >= t)      return "Sedang";
  else                  return "Tinggi";
}
```
Membandingkan ketiga nilai μ dan mengambil yang terbesar sebagai label.
Untuk x = 25.4: r=0.533 terbesar → kembalikan **"Rendah"**

---

### Fungsi Inferensi (Inti Sistem Fuzzy)

```cpp
FuzzyOutput inferensiFuzzy(float suhu, float soil, float pH) {
  FuzzyOutput out;
```
Fungsi menerima 3 input sensor dan menghasilkan keputusan relay.

```cpp
  float mu_st = fuzzyTempTinggi(suhu);   // = 0.000
  float mu_tk = fuzzySoilKering(soil);   // = 0.000
  float mu_tl = fuzzySoilLembab(soil);   // = 1.000
  float mu_pa = fuzzyPhAsam(pH);         // = 0.550
  float mu_pb = fuzzyPhBasa(pH);         // = 0.000
```
Hitung semua derajat keanggotaan yang dipakai dalam rule.

```cpp
  // Rule Kipas
  out.mu_kipas = mu_st;            // = 0.000
  out.kipas    = (mu_st > 0.5f);   // false → OFF
```
Kipas menyala hanya jika μ_Tinggi lebih dari 0.5.

```cpp
  // Rule Pompa Air
  float r2 = mu_tk;                      // 0.000 (kering)
  float r3 = min(mu_tl, mu_st);          // min(1.0, 0.0) = 0.000
  out.mu_pompa_air = max(r2, r3);        // max(0, 0) = 0.000
  out.pompa_air    = (out.mu_pompa_air > 0.4f); // false → OFF
```
- `min()` untuk operator AND (kedua syarat harus terpenuhi)
- `max()` untuk operator OR (salah satu syarat cukup)

```cpp
  // Rule Pompa pH (hanya Asam yang mengaktifkan)
  out.mu_pompa_ph = mu_pa;              // = 0.550
  out.pompa_ph    = (mu_pa > 0.4f);    // true → ON ✅
```
Pompa pH menyala karena μ_Asam = 0.55 melewati threshold 0.4.

---

## Ringkasan Alur Lengkap

```
INPUT SENSOR
  Suhu = 25.4°C  |  Soil = 64%  |  pH = 5.45
          ↓
FUZZIFIKASI
  μ_Rendah = 0.533  μ_Sedang = 0.467  μ_Tinggi = 0.000
  μ_Kering = 0.000  μ_Lembab = 1.000  μ_Basah  = 0.000
  μ_Asam   = 0.550  μ_Normal = 0.000  μ_Basa   = 0.000
          ↓
INFERENSI (AND=min, OR=max)
  Kipas    = μ_Tinggi = 0.000
  PompaAir = max(μ_Kering, min(μ_Lembab, μ_Tinggi))
           = max(0, min(1.0, 0)) = 0.000
  PompaPH  = μ_Asam = 0.550
          ↓
DEFUZZIFIKASI (threshold)
  Kipas    : 0.000 ≤ 0.5  → OFF ❌
  PompaAir : 0.000 ≤ 0.4  → OFF ❌
  PompaPH  : 0.550 > 0.4  → ON  ✅
          ↓
OUTPUT AKTUATOR
  🌀 Kipas Pendingin  → MATI
  💦 Pompa Irigasi    → MATI
  🧪 Pompa Koreksi pH → HIDUP (menambah larutan agar pH naik ke normal)
```

---

*File ini menjelaskan implementasi Fuzzy Tahani pada sistem monitoring greenhouse cabai rawit berbasis ESP32.*
