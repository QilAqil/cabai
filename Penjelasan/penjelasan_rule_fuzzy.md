# Penjelasan Rule Base & Contoh Perhitungan
## Metode Fuzzy Tahani — Sistem Pertanian Cerdas Cabai Rawit

---

## 1. Apa Itu Rule Base Fuzzy Tahani?

Rule base adalah kumpulan aturan **IF–THEN** yang menghubungkan kondisi
lingkungan tanaman (input) dengan tindakan aktuator (output).

Pada metode Fuzzy Tahani, setiap aturan dievaluasi menggunakan
**fire strength** — yaitu nilai derajat keanggotaan yang menjadi
ukuran seberapa kuat suatu kondisi terpenuhi.

```
Alur kerja:
INPUT SENSOR → FUZZIFIKASI → EVALUASI RULE → FIRE STRENGTH → KEPUTUSAN AKTUATOR
```

---

## 2. Tiga Rule yang Digunakan

### Rule 1 — Kipas Pendingin

```
IF  suhu udara adalah TINGGI
THEN  kipas pendingin ON
```

| Elemen | Penjelasan |
|--------|-----------|
| Kondisi | Suhu udara masuk himpunan TINGGI |
| Fire Strength | μ_Tinggi(suhu) |
| Threshold | > **0.50** |
| Aktuator | 🌀 Kipas Pendingin |
| Logika | Suhu > 31°C berbahaya bagi bunga cabai rawit → perlu pendinginan segera |

**Fungsi keanggotaan TINGGI (trapesium bahu kanan):**
```
         0         jika suhu ≤ 27°C
μ_Tinggi =  (suhu − 27) / 4   jika 27°C < suhu < 31°C
         1         jika suhu ≥ 31°C
```

---

### Rule 2 — Pompa Air Irigasi

```
IF  kelembapan tanah adalah KERING
THEN  pompa air (irigasi) ON
```

| Elemen | Penjelasan |
|--------|-----------|
| Kondisi | Kelembapan tanah masuk himpunan KERING |
| Fire Strength | μ_Kering(tanah) |
| Threshold | > **0.40** |
| Aktuator | 💦 Pompa Air |
| Logika | Tanah < 40% kekurangan air → akar tidak bisa serap nutrisi optimal |

**Fungsi keanggotaan KERING (trapesium bahu kiri):**
```
         1         jika tanah ≤ 40%
μ_Kering =  (50 − tanah) / 10  jika 40% < tanah < 50%
         0         jika tanah ≥ 50%
```

---

### Rule 3 — Pompa Koreksi pH

```
IF  pH tanah adalah ASAM  ATAU  pH tanah adalah BASA
THEN  pompa koreksi pH ON
```

| Elemen | Penjelasan |
|--------|-----------|
| Kondisi | pH tanah masuk himpunan ASAM atau BASA |
| Fire Strength | **max**(μ_Asam, μ_Basa) — operator OR = nilai terbesar |
| Threshold | > **0.40** |
| Aktuator | 🧪 Pompa Koreksi pH |
| Logika | pH < 6 atau pH > 7 menghambat penyerapan hara → perlu koreksi dolomit/larutan |

**Fungsi keanggotaan ASAM (trapesium bahu kiri):**
```
         1          jika pH ≤ 5.0
μ_Asam =  (6 − pH) / 1    jika 5.0 < pH < 6.0
         0          jika pH ≥ 6.0
```

**Fungsi keanggotaan BASA (trapesium bahu kanan):**
```
         0          jika pH ≤ 7.0
μ_Basa =  (pH − 7) / 0.5  jika 7.0 < pH < 7.5
         1          jika pH ≥ 7.5
```

---

## 3. Ringkasan Rule Base

```
┌────┬─────────────────────────────┬──────────────────────────────┬───────────┐
│Rule│ Kondisi (IF)                │ Fire Strength                │ Aktuator  │
├────┼─────────────────────────────┼──────────────────────────────┼───────────┤
│ R1 │ Suhu TINGGI                 │ μ_Tinggi(T)        > 0.50   │ 🌀 Kipas  │
│ R2 │ Tanah KERING                │ μ_Kering(S)        > 0.40   │ 💦 PompaAir│
│ R3 │ pH ASAM  ATAU  pH BASA      │ max(μ_Asam, μ_Basa)> 0.40   │ 🧪 PompaPH│
└────┴─────────────────────────────┴──────────────────────────────┴───────────┘
```

**Catatan penting:**
- Setiap rule bersifat **independen** — bisa aktif bersamaan (misal R2 + R3)
- Operator R3 menggunakan **OR (MAX)** karena pH asam DAN basa sama-sama berbahaya
- Threshold R1 lebih tinggi (0.50) karena kipas bekerja terus-menerus, bukan timer

---

## 4. Contoh Perhitungan Lengkap

### Contoh A — Kondisi Normal (Semua Aktuator OFF)

**Input sensor:**
```
Suhu   = 24.4°C
Tanah  = 49%
pH     = 5.95
```

**Langkah 1 — Fuzzifikasi Suhu**
```
μ_Rendah = (27 − 24.4) / 3 = 2.6 / 3 = 0.87
μ_Sedang = (24.4 − 24) / 3 = 0.4 / 3 = 0.13
μ_Tinggi = 0  (karena 24.4 ≤ 27)
Himpunan dominan → RENDAH (0.87)
```

**Langkah 2 — Fuzzifikasi Kelembapan Tanah**
```
μ_Kering = (50 − 49) / 10 = 1 / 10 = 0.10
μ_Lembab = (49 − 40) / 10 = 9 / 10 = 0.90
μ_Basah  = 0  (karena 49 ≤ 70)
Himpunan dominan → LEMBAB (0.90)
```

**Langkah 3 — Fuzzifikasi pH**
```
μ_Asam   = (6 − 5.95) / 1  = 0.05 / 1 = 0.05
μ_Normal = (5.95 − 5.5) / 0.5 = 0.45 / 0.5 = 0.90
μ_Basa   = 0  (karena 5.95 ≤ 7.0)
Himpunan dominan → NORMAL (0.90)
```

**Langkah 4 — Evaluasi Rule & Fire Strength**
```
R1: μ_Kipas     = μ_Tinggi(24.4)         = 0.00
R2: μ_Pompa Air = μ_Kering(49)           = 0.10
R3: μ_Pompa pH  = max(μ_Asam, μ_Basa)
                = max(0.05, 0.00)         = 0.05
```

**Langkah 5 — Keputusan Aktuator**
```
R1: 0.00 > 0.50? TIDAK → Kipas     OFF ✗
R2: 0.10 > 0.40? TIDAK → Pompa Air OFF ✗
R3: 0.05 > 0.40? TIDAK → Pompa pH  OFF ✗
```

> ✅ Semua aktuator OFF — kondisi mendekati optimal, sistem tidak perlu intervene.

---

### Contoh B — Suhu Terlalu Tinggi (Kipas ON)

**Input sensor:**
```
Suhu   = 32.0°C
Tanah  = 70%
pH     = 6.50
```

**Langkah 1 — Fuzzifikasi Suhu**
```
μ_Tinggi = 1.0  (karena 32 ≥ 31)
Himpunan dominan → TINGGI (1.00)
```

**Langkah 2 — Fuzzifikasi Tanah**
```
μ_Lembab = 1.0  (karena 50 ≤ 70 ≤ 70)
μ_Kering = 0.0
Himpunan dominan → LEMBAB (1.00)
```

**Langkah 3 — Fuzzifikasi pH**
```
μ_Normal = 1.0  (karena 6 ≤ 6.5 ≤ 7)
μ_Asam   = 0.0
μ_Basa   = 0.0
Himpunan dominan → NORMAL (1.00)
```

**Langkah 4 — Fire Strength**
```
R1: μ_Kipas     = μ_Tinggi(32.0) = 1.00
R2: μ_Pompa Air = μ_Kering(70)   = 0.00
R3: μ_Pompa pH  = max(0.00, 0.00)= 0.00
```

**Langkah 5 — Keputusan**
```
R1: 1.00 > 0.50? YA  → 🌀 Kipas     ON  ✓
R2: 0.00 > 0.40? TIDAK → Pompa Air OFF ✗
R3: 0.00 > 0.40? TIDAK → Pompa pH  OFF ✗
```

> ✅ Kipas menyala untuk menurunkan suhu greenhouse.

---

### Contoh C — Tanah Kering (Pompa Air ON)

**Input sensor:**
```
Suhu   = 22.0°C
Tanah  = 35%
pH     = 6.50
```

**Fuzzifikasi Tanah:**
```
μ_Kering = 1.0  (karena 35 ≤ 40)
Himpunan dominan → KERING (1.00)
```

**Fire Strength:**
```
R2: μ_Pompa Air = μ_Kering(35) = 1.00
```

**Keputusan:**
```
R2: 1.00 > 0.40? YA → 💦 Pompa Air ON ✓
    Nyala 10 detik, lalu jeda 30 menit
```

---

### Contoh D — pH Asam (Pompa pH ON)

**Input sensor:**
```
Suhu   = 22.0°C
Tanah  = 70%
pH     = 4.80
```

**Fuzzifikasi pH:**
```
μ_Asam  = 1.0  (karena 4.80 ≤ 5.0)
μ_Basa  = 0.0
```

**Fire Strength:**
```
R3: μ_Pompa pH = max(1.00, 0.00) = 1.00
```

**Keputusan:**
```
R3: 1.00 > 0.40? YA → 🧪 Pompa pH ON ✓
    Nyala 10 detik, lalu jeda 3 jam
```

---

### Contoh E — pH Basa (Pompa pH ON)

**Input sensor:**
```
Suhu   = 22.0°C
Tanah  = 58%
pH     = 8.00
```

**Fuzzifikasi pH:**
```
μ_Basa  = 1.0  (karena 8.00 ≥ 7.5)
μ_Asam  = 0.0
```

**Fire Strength:**
```
R3: μ_Pompa pH = max(0.00, 1.00) = 1.00
```

**Keputusan:**
```
R3: 1.00 > 0.40? YA → 🧪 Pompa pH ON ✓
```

> ✅ Rule 3 menangani DUA kondisi berbahaya (asam & basa) dalam satu aturan
> menggunakan operator OR (MAX).

---

### Contoh F — Kombinasi R2 + R3 (Dua Aktuator ON)

**Input sensor:**
```
Suhu   = 22.0°C
Tanah  = 35%
pH     = 5.20
```

**Fuzzifikasi:**
```
Suhu:
  μ_Tinggi = 0.00  (22 ≤ 27)

Tanah:
  μ_Kering = 1.00  (35 ≤ 40)

pH:
  μ_Asam  = (6 − 5.20) / 1 = 0.80
  μ_Basa  = 0.00
```

**Fire Strength:**
```
R1: μ_Kipas     = 0.00
R2: μ_Pompa Air = μ_Kering(35) = 1.00
R3: μ_Pompa pH  = max(0.80, 0.00) = 0.80
```

**Keputusan:**
```
R1: 0.00 > 0.50? TIDAK → Kipas     OFF ✗
R2: 1.00 > 0.40? YA    → 💦 Pompa Air ON ✓
R3: 0.80 > 0.40? YA    → 🧪 Pompa pH  ON ✓
```

> ✅ Dua aktuator aktif bersamaan — tanah kering sekaligus pH asam.
> Ini membuktikan rule bersifat independen dan paralel.

---

### Contoh G — Suhu Parsial (μ = 0.75, Kipas ON)

**Input sensor:**
```
Suhu   = 30.0°C
Tanah  = 70%
pH     = 6.50
```

**Fuzzifikasi Suhu:**
```
μ_Sedang = (31 − 30) / 4 = 0.25
μ_Tinggi = (30 − 27) / 4 = 3 / 4 = 0.75
Himpunan dominan → TINGGI (0.75)
```

**Fire Strength:**
```
R1: μ_Kipas = μ_Tinggi(30) = 0.75
```

**Keputusan:**
```
R1: 0.75 > 0.50? YA → 🌀 Kipas ON ✓
```

> ✅ Suhu belum mencapai 31°C, tetapi sudah cukup tinggi (μ = 0.75)
> untuk mengaktifkan kipas. Inilah keunggulan fuzzy dibanding logika biner:
> **"suhu 30°C sudah 75% masuk kategori TINGGI"**.

---

### Contoh H — Uji Tepat di Batas Threshold (Edge Case)

**Input sensor:**
```
Suhu   = 22.0°C
Tanah  = 45%
pH     = 5.60
```

**Fuzzifikasi:**
```
Tanah:
  μ_Kering = (50 − 45) / 10 = 0.50
  μ_Lembab = (45 − 40) / 10 = 0.50

pH:
  μ_Asam   = (6 − 5.60) / 1 = 0.40
  μ_Normal = (5.60 − 5.5) / 0.5 = 0.20
  μ_Basa   = 0.00
```

**Fire Strength:**
```
R2: μ_Pompa Air = μ_Kering(45) = 0.50
R3: μ_Pompa pH  = max(0.40, 0.00) = 0.40
```

**Keputusan:**
```
R2: 0.50 > 0.40? YA    → 💦 Pompa Air ON  ✓
R3: 0.40 > 0.40? TIDAK → Pompa pH     OFF ✗
```

> ⚠️ Kasus penting: μ = **0.40 TIDAK memenuhi** threshold `> 0.40` (ketat).
> μ = **0.41** baru akan memenuhi. Ini membuktikan threshold bersifat **eksklusif**,
> bukan inklusif (pakai `>`, bukan `≥`).

---

## 5. Visualisasi Fungsi Keanggotaan

### Suhu Udara

```
μ   1 ┤████████████████
      │                ╲Rendah     Sedang╱╲  Tinggi
  0.5 ┤                  ╲          ╱  ╲  ╲.........
      │                    ╲      ╱    ╲  ╲
   0  ┼────────────────────────────────────────────→ Suhu (°C)
      0         24    27        31              45
```

| Zona | Deskripsi |
|------|----------|
| ≤ 24°C | Rendah = 1.00 (terlalu sejuk) |
| 24–27°C | Transisi Rendah → Sedang |
| 27°C | Puncak Sedang |
| 27–31°C | Transisi Sedang → Tinggi |
| ≥ 31°C | Tinggi = 1.00 (terlalu panas) |

### Kelembapan Tanah

```
μ   1 ┤████████
      │        ╲Kering    Lembab████████╲  Basah
  0.5 ┤          ╲      ╱               ╲  ╱.....
      │            ╲  ╱                   ╲╱
   0  ┼────────────────────────────────────────────→ Tanah (%)
      0    40    50            70    80         100
```

| Zona | Deskripsi |
|------|----------|
| ≤ 40% | Kering = 1.00 (kekurangan air) |
| 40–50% | Transisi Kering → Lembab |
| 50–70% | Lembab = 1.00 (optimal) |
| 70–80% | Transisi Lembab → Basah |
| ≥ 80% | Basah = 1.00 (kelebihan air) |

### pH Tanah

```
μ   1 ┤███████╲          Normal████████╲      ╱ Basa
      │        ╲Asam    ╱               ╲    ╱
  0.5 ┤          ╲    ╱                   ╲╱.......
      │            ╲╱╲                     ╲
   0  ┼────────────────────────────────────────────→ pH
      3    5  5.5  6              7  7.5         9
```

| Zona | Deskripsi |
|------|----------|
| ≤ 5.0 | Asam = 1.00 (terlalu asam) |
| 5.0–6.0 | Transisi Asam → Normal |
| 6.0–7.0 | Normal = 1.00 (optimal cabai) |
| 7.0–7.5 | Transisi Normal → Basa |
| ≥ 7.5 | Basa = 1.00 (terlalu basa) |

---

## 6. Mengapa Menggunakan Fuzzy Dibanding Logika Biasa?

**Logika Biner (IF-ELSE biasa):**
```
IF suhu > 31°C THEN kipas ON
ELSE kipas OFF
```
Masalah: suhu 30.9°C → OFF, suhu 31.1°C → ON. Perbedaan 0.2°C
menghasilkan keputusan berbeda secara drastis.

**Logika Fuzzy (Fuzzy Tahani):**
```
Suhu 30.0°C → μ_Tinggi = 0.75 → Kipas ON (75% "tinggi")
Suhu 31.0°C → μ_Tinggi = 1.00 → Kipas ON (100% "tinggi")
Suhu 28.0°C → μ_Tinggi = 0.25 → Kipas OFF (25% "tinggi", belum cukup)
```

Keputusan **bersifat gradual**, tidak tiba-tiba berubah — lebih sesuai
dengan pola penalaran manusia dan kondisi pertanian nyata.

---

## 7. Mengapa Threshold R1 = 0.50 Berbeda dari R2/R3 = 0.40?

| Rule | Threshold | Alasan |
|------|-----------|--------|
| R1 Kipas | > 0.50 | Kipas bekerja terus-menerus saat aktif — lebih boros energi, perlu keyakinan lebih tinggi bahwa suhu benar-benar tinggi |
| R2 Pompa Air | > 0.40 | Tanaman lebih sensitif terhadap kekurangan air — lebih baik menyiram sedikit terlambat daripada tanaman layu |
| R3 Pompa pH | > 0.40 | pH yang menyimpang bahkan sedikit dari optimal sudah menghambat penyerapan hara |

---

## 8. Logika Timer Pompa (Proteksi Anti-Boros)

Rule 2 dan Rule 3 tidak serta-merta menyalakan pompa terus-menerus.
Sistem mengimplementasikan **logika timer dua jalur**:

```
Jalur AUTO (dari fuzzy):
  Pompa Air: ON 10 detik → OFF → jeda 30 menit → evaluasi ulang
  Pompa pH : ON 10 detik → OFF → jeda 3 jam    → evaluasi ulang

Jalur MANUAL (dari dashboard):
  Pompa langsung ON selama switch manual aktif
  Jeda AUTO tidak terpengaruh
```

**Mengapa ada jeda?**
- Pompa Air: tanah membutuhkan waktu menyerap air — 10 detik sudah cukup untuk prototype
- Pompa pH: perubahan pH tanah berlangsung lambat — terlalu sering koreksi bisa membuat pH berfluktuasi

---

## 9. Alur Lengkap dalam Kode ESP32

```
taskSensor (Core 1) — setiap 15 detik:

1. Baca DHT22 → suhu, kelembapan_udara
2. Baca Soil (50 sampel Kalman) → kelembapan_tanah
3. Jeda isolasi galvanik 4 detik
4. Baca pH (150 sampel Kalman + drift-guard) → pH_tanah

5. inferensiFuzzy(suhu, kelembapan_tanah, pH_tanah):
   ├─ μ_Tinggi = fuzzyTempTinggi(suhu)
   ├─ μ_Kering = fuzzySoilKering(kelembapan_tanah)
   ├─ μ_Asam   = fuzzyPhAsam(pH_tanah)
   ├─ μ_Basa   = fuzzyPhBasa(pH_tanah)
   │
   ├─ R1: kipas    = (μ_Tinggi > 0.5)
   ├─ R2: pompa_air= (μ_Kering > 0.4) AND (tanah > 0)
   └─ R3: pompa_ph = (max(μ_Asam, μ_Basa) > 0.4) AND (pH > 0)

6. terapkanAktuator(kipas, pompa_air, pompa_ph):
   ├─ Kipas    → setRelay(GPIO25, kipas)   [langsung]
   ├─ Pompa Air→ reqPompaAir = pompa_air   [via tickPompa()]
   └─ Pompa pH → reqPompaPH  = pompa_ph    [via tickPompa()]

7. tickPompa() setiap 20ms:
   └─ Cek jeda → nyalakan/matikan pompa sesuai timer
```

---

## 10. Ringkasan Cepat untuk Presentasi

```
╔════════════════════════════════════════════════════════════╗
║         RULE BASE FUZZY TAHANI — RINGKASAN                 ║
╠════╦═══════════════════════╦══════════════╦════════════════╣
║ R1 ║ Suhu > 31°C (TINGGI)  ║ μ > 0.50     ║ 🌀 KIPAS ON   ║
║ R2 ║ Tanah < 40% (KERING)  ║ μ > 0.40     ║ 💦 POMPA AIR  ║
║ R3 ║ pH < 6 atau pH > 7.5  ║ max(μ)> 0.40 ║ 🧪 POMPA pH   ║
╚════╩═══════════════════════╩══════════════╩════════════════╝

Contoh cepat:
  Suhu=32°C → μ_Tinggi=1.00 > 0.50 → KIPAS ON
  Tanah=35% → μ_Kering=1.00 > 0.40 → POMPA AIR ON (10 dtk, jeda 30 mnt)
  pH=4.80   → μ_Asam  =1.00 > 0.40 → POMPA pH  ON (10 dtk, jeda 3 jam)
  pH=8.00   → μ_Basa  =1.00 > 0.40 → POMPA pH  ON
```

---

*Dokumen ini menjelaskan rule base dan contoh perhitungan Fuzzy Tahani
pada sistem pertanian cerdas cabai rawit — disesuaikan dengan implementasi
aktual di `index/index.ino`.*
