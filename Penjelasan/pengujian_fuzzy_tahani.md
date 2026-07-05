# Pengujian Metode Fuzzy Tahani
## Sistem Pertanian Cerdas Berbasis IoT — Greenhouse Cabai Rawit

---

Pada bagian ini dilakukan pengujian terhadap metode Fuzzy Tahani yang
diterapkan pada sistem pertanian cerdas berbasis Internet of Things.
Pengujian bertujuan untuk mengetahui kesesuaian hasil perhitungan metode
Fuzzy Tahani yang dihasilkan oleh sistem dengan perhitungan secara manual.

Proses pengujian dilakukan dengan memasukkan nilai parameter kondisi
lingkungan tanaman cabai rawit, yaitu pH tanah, suhu udara, dan kelembapan
tanah sebagai data masukan. Selanjutnya, sistem menghitung nilai derajat
keanggotaan, melakukan pembentukan query fuzzy, menghitung fire strength,
dan menghasilkan keputusan pengendalian kondisi lingkungan.

Hasil keluaran dari sistem kemudian dibandingkan dengan hasil perhitungan
manual untuk memastikan bahwa proses pengambilan keputusan menggunakan
metode Fuzzy Tahani telah berjalan sesuai dengan aturan yang telah
dirancang.

---

## Tabel Himpunan Fuzzy

| Variabel | Himpunan | Bawah | Tengah | Atas |
|----------|----------|-------|--------|------|
| pH Tanah | Asam     | 3   | 5   | 6   |
| pH Tanah | Normal   | 5.5 | 6.5 | 7.5 |
| pH Tanah | Basa     | 7   | 7.5 | 9   |
| Suhu Udara | Rendah | 0  | 24  | 27  |
| Suhu Udara | Sedang | 24 | 27  | 31  |
| Suhu Udara | Tinggi | 27 | 31  | 45  |
| Kelembapan Tanah | Kering | 0  | 40 | 50  |
| Kelembapan Tanah | Lembab | 40 | 60 | 80  |
| Kelembapan Tanah | Basah  | 70 | 80 | 100 |

---

## Data Masukan dari Sensor

Nilai pH tanah, suhu udara, dan kelembapan tanah yang diperoleh dari
sensor dapat dilihat pada tabel berikut:

| No | Tanggal | pH Tanah | Suhu (°C) | Kelembapan Tanah (%) |
|----|---------|----------|-----------|----------------------|
| 1  | 03/7/2026 13:25 | 6.90 | 22.0 | 70 |
| 2  | 03/7/2026 13:24 | 6.90 | 22.0 | 71 |
| 3  | 04/7/2026 13:23 | 6.43 | 23.1 | 58 |
| 4  | 04/7/2026 13:22 | 6.31 | 23.1 | 58 |
| 5  | 05/7/2026 13:21 | 5.95 | 24.4 | 49 |

**Kelompok B — Skenario ekstrem (simulasi / uji rule base):**

| No | Tanggal | pH Tanah | Suhu (°C) | Kelembapan Tanah (%) | Tujuan uji |
|----|---------|----------|-----------|----------------------|------------|
| 6  | 05/7/2026 14:00 | 4.80 | 22.0 | 70 | R4 — pH asam → Pompa pH ON |
| 7  | 05/7/2026 14:01 | 6.50 | 32.0 | 70 | R1 — suhu tinggi → Kipas ON |
| 8  | 05/7/2026 14:02 | 6.50 | 22.0 | 35 | R2 — tanah kering → Pompa Air ON |
| 9  | 05/7/2026 14:03 | 6.50 | 32.0 | 55 | R3 — lembab AND tinggi → Pompa Air ON |
| 10 | 05/7/2026 14:04 | 5.20 | 22.0 | 35 | R2 + R4 → Pompa Air & Pompa pH ON |
| 11 | 05/7/2026 14:05 | 6.50 | 30.0 | 70 | R1 — suhu agak tinggi (μ Tinggi = 0,75) |
| 12 | 05/7/2026 14:06 | 5.60 | 22.0 | 45 | Ambang — μ Asam = 0,40 (OFF), μ Kering = 0,50 (ON) |

---

## Pengujian Manual Menggunakan Microsoft Excel

Perhitungan dilakukan menggunakan Microsoft Excel dengan cara
memasukkan rumus IF bertingkat pada setiap kolom derajat keanggotaan.
Struktur kolom pada Excel adalah sebagai berikut:

```
Kolom A = No
Kolom B = Tanggal
Kolom C = pH Tanah
Kolom D = Suhu Udara (°C)
Kolom E = Kelembapan Tanah (%)
Kolom F = μ Asam
Kolom G = μ Normal
Kolom H = μ Basa
Kolom I = Hasil pH
Kolom J = μ Rendah
Kolom K = μ Sedang
Kolom L = μ Tinggi
Kolom M = Hasil Suhu
Kolom N = μ Kering
Kolom O = μ Lembab
Kolom P = μ Basah
Kolom Q = Hasil Tanah
```

---

## a. Fuzzifikasi pada Nilai pH Tanah

Asumsikan data pH berada di kolom C, mulai dari baris 2.

➢ Nilai keanggotaan Asam
```excel
=IF(C2<=5, 1, IF(C2<=6, (6-C2)/(6-5), 0))
```

➢ Nilai keanggotaan Normal
```excel
=IF(C2<=5.5, 0, IF(C2<=6, (C2-5.5)/(6-5.5),
  IF(C2<=7, 1, IF(C2<=7.5, (7.5-C2)/(7.5-7), 0))))
```

➢ Nilai keanggotaan Basa
```excel
=IF(C2<=7, 0, IF(C2<=7.5, (C2-7)/(7.5-7), 1))
```

➢ Hasil (himpunan dominan)
```excel
=IF(AND(F2>=G2,F2>=H2),"Asam",IF(G2>=H2,"Normal","Basa"))
```

### Tabel Derajat Keanggotaan pH Tanah

| No | pH | μ Asam | μ Normal | μ Basa | Hasil |
|----|----|--------|----------|--------|-------|
| 1 | 6.90 | IF(6.90≤5,1,...) → 6.90>6 → **0.00** | IF(6.90≤7,1) → **1.00** | IF(6.90≤7) → **0.00** | **Normal** |
| 2 | 6.90 | **0.00** | **1.00** | **0.00** | **Normal** |
| 3 | 6.43 | **0.00** | IF(6.43≤7,1) → **1.00** | **0.00** | **Normal** |
| 4 | 6.31 | **0.00** | IF(6.31≤7,1) → **1.00** | **0.00** | **Normal** |
| 5 | 5.95 | IF(5.95≤6, (6-5.95)/1) = **0.05** | IF(5.95≤6, (5.95-5.5)/0.5) = **0.90** | **0.00** | **Normal** |

**Cara menghitung manual (No. 1, pH = 6.90):**
```
μ_Asam   : pH=6.90 > 6 → μ = 0.00
μ_Normal : pH=6.90, 6 < 6.90 ≤ 7 → μ = 1.00 ✓
μ_Basa   : pH=6.90 ≤ 7 → μ = 0.00
Hasil    : argmax(0.00, 1.00, 0.00) = Normal
```

**Cara menghitung manual (No. 5, pH = 5.95):**
```
μ_Asam   : pH=5.95, 5 < 5.95 ≤ 6 → μ = (6 - 5.95) / (6 - 5) = 0.05 / 1 = 0.05
μ_Normal : pH=5.95, 5.5 < 5.95 ≤ 6 → μ = (5.95 - 5.5) / (6 - 5.5) = 0.45 / 0.5 = 0.90 ✓
μ_Basa   : pH=5.95 ≤ 7 → μ = 0.00
Hasil    : argmax(0.05, 0.90, 0.00) = Normal
```

---

## b. Fuzzifikasi pada Nilai Suhu Udara

Asumsikan data suhu berada di kolom D, mulai dari baris 2.

➢ Nilai keanggotaan Rendah
```excel
=IF(D2<=24, 1, IF(D2<=27, (27-D2)/(27-24), 0))
```

➢ Nilai keanggotaan Sedang
```excel
=IF(D2<=24, 0, IF(D2<=27, (D2-24)/(27-24),
  IF(D2<=31, (31-D2)/(31-27), 0)))
```

➢ Nilai keanggotaan Tinggi
```excel
=IF(D2<=27, 0, IF(D2<=31, (D2-27)/(31-27), 1))
```

➢ Hasil (himpunan dominan)
```excel
=IF(AND(J2>=K2,J2>=L2),"Rendah",IF(K2>=L2,"Sedang","Tinggi"))
```

### Tabel Derajat Keanggotaan Suhu Udara

| No | Suhu | μ Rendah | μ Sedang | μ Tinggi | Hasil |
|----|------|----------|----------|----------|-------|
| 1 | 22.0 | IF(22.0≤24,1) = **1.00** | 0.00 | 0.00 | **Rendah** |
| 2 | 22.0 | **1.00** | 0.00 | 0.00 | **Rendah** |
| 3 | 23.1 | IF(23.1≤24,1) = **1.00** | 0.00 | 0.00 | **Rendah** |
| 4 | 23.1 | **1.00** | 0.00 | 0.00 | **Rendah** |
| 5 | 24.4 | IF(24<24.4≤27, (27-24.4)/3) = **0.87** | IF(24<24.4≤27, (24.4-24)/3) = **0.13** | 0.00 | **Rendah** |

**Cara menghitung manual (No. 1, Suhu = 22.0°C):**
```
μ_Rendah : 22.0 ≤ 24 → μ = 1.00 ✓
μ_Sedang : 22.0 ≤ 24 → μ = 0.00
μ_Tinggi : 22.0 ≤ 27 → μ = 0.00
Hasil    : argmax(1.00, 0.00, 0.00) = Rendah
```

**Cara menghitung manual (No. 5, Suhu = 24.4°C):**
```
μ_Rendah : 24 < 24.4 ≤ 27 → μ = (27 - 24.4) / (27 - 24) = 2.6 / 3 = 0.87 ✓
μ_Sedang : 24 < 24.4 ≤ 27 → μ = (24.4 - 24) / (27 - 24) = 0.4 / 3 = 0.13
μ_Tinggi : 24.4 ≤ 27 → μ = 0.00
Hasil    : argmax(0.87, 0.13, 0.00) = Rendah
```

---

## c. Fuzzifikasi pada Nilai Kelembapan Tanah

Asumsikan data kelembapan tanah berada di kolom E, mulai dari baris 2.

➢ Nilai keanggotaan Kering
```excel
=IF(E2<=40, 1, IF(E2<=50, (50-E2)/(50-40), 0))
```

➢ Nilai keanggotaan Lembab
```excel
=IF(E2<=40, 0, IF(E2<=50, (E2-40)/(50-40),
  IF(E2<=70, 1, IF(E2<=80, (80-E2)/(80-70), 0))))
```

➢ Nilai keanggotaan Basah
```excel
=IF(E2<=70, 0, IF(E2<=80, (E2-70)/(80-70), 1))
```

➢ Hasil (himpunan dominan)
```excel
=IF(AND(N2>=O2,N2>=P2),"Kering",IF(O2>=P2,"Lembab","Basah"))
```

### Tabel Derajat Keanggotaan Kelembapan Tanah

| No | Tanah (%) | μ Kering | μ Lembab | μ Basah | Hasil |
|----|-----------|----------|----------|---------|-------|
| 1 | 70 | 0.00 | IF(50<70≤70, 1) = **1.00** | IF(70≤70) = **0.00** | **Lembab** |
| 2 | 71 | 0.00 | IF(70<71≤80, (80-71)/10) = **0.90** | IF(70<71≤80, (71-70)/10) = **0.10** | **Lembab** |
| 3 | 58 | 0.00 | IF(50<58≤70, 1) = **1.00** | **0.00** | **Lembab** |
| 4 | 58 | 0.00 | **1.00** | **0.00** | **Lembab** |
| 5 | 49 | IF(40<49≤50, (50-49)/10) = **0.10** | IF(40<49≤50, (49-40)/10) = **0.90** | **0.00** | **Lembab** |

**Cara menghitung manual (No. 1, Tanah = 70%):**
```
μ_Kering : 70 > 50 → μ = 0.00
μ_Lembab : 50 < 70 ≤ 70 → μ = 1.00 ✓
μ_Basah  : 70 ≤ 70 → μ = 0.00
Hasil    : argmax(0.00, 1.00, 0.00) = Lembab
```

**Cara menghitung manual (No. 2, Tanah = 71%):**
```
μ_Kering : 71 > 50 → μ = 0.00
μ_Lembab : 70 < 71 ≤ 80 → μ = (80 - 71) / (80 - 70) = 9 / 10 = 0.90 ✓
μ_Basah  : 70 < 71 ≤ 80 → μ = (71 - 70) / (80 - 70) = 1 / 10 = 0.10
Hasil    : argmax(0.00, 0.90, 0.10) = Lembab
```

**Cara menghitung manual (No. 5, Tanah = 49%):**
```
μ_Kering : 40 < 49 ≤ 50 → μ = (50 - 49) / (50 - 40) = 1 / 10 = 0.10
μ_Lembab : 40 < 49 ≤ 50 → μ = (49 - 40) / (50 - 40) = 9 / 10 = 0.90 ✓
μ_Basah  : 49 ≤ 70 → μ = 0.00
Hasil    : argmax(0.10, 0.90, 0.00) = Lembab
```

---

## d. Pembentukan Query Fuzzy dan Fire Strength

Setelah fuzzifikasi, sistem membentuk query fuzzy berdasarkan
rule base yang telah dirancang:

| No | Rule | Kondisi | Operator | Fire Strength | Aktuator |
|----|------|---------|----------|---------------|----------|
| R1 | IF suhu Tinggi | μ_Tinggi | — | μ_Tinggi | KIPAS |
| R2 | IF tanah Kering | μ_Kering | — | μ_Kering | POMPA AIR |
| R3 | IF tanah Lembab AND suhu Tinggi | μ_Lembab, μ_Tinggi | MIN | min(μ_Lembab, μ_Tinggi) | POMPA AIR |
| R4 | IF pH Asam | μ_Asam | — | μ_Asam | POMPA pH |

**Rumus Excel untuk Fire Strength:**
```excel
Kipas    : =L2                           (μ_Tinggi)
PompaAir : =MAX(N2, MIN(O2, L2))         (OR dari R2 dan R3)
PompaPH  : =F2                           (μ_Asam)
```

### Tabel Fire Strength dan Keputusan Aktuator

| No | μ Kipas | μ Pompa Air | μ Pompa pH | Kipas | Pompa Air | Pompa pH |
|----|---------|-------------|-----------|-------|-----------|---------| 
| 1 | 0.00 | max(0, min(1.00, 0)) = **0.00** | **0.00** | OFF | OFF | OFF |
| 2 | 0.00 | max(0, min(0.90, 0)) = **0.00** | **0.00** | OFF | OFF | OFF |
| 3 | 0.00 | max(0, min(1.00, 0)) = **0.00** | **0.00** | OFF | OFF | OFF |
| 4 | 0.00 | max(0, min(1.00, 0)) = **0.00** | **0.00** | OFF | OFF | OFF |
| 5 | 0.00 | max(0.10, min(0.90, 0)) = **0.10** | **0.05** | OFF | OFF | OFF |
| 6 | 0.00 | 0.00 | **1.00** | OFF | OFF | **ON** |
| 7 | **1.00** | max(0, min(1.00, 1.00)) = **1.00** | 0.00 | **ON** | **ON** | OFF |
| 8 | 0.00 | **1.00** | 0.00 | OFF | **ON** | OFF |
| 9 | **1.00** | max(0, min(1.00, 1.00)) = **1.00** | 0.00 | **ON** | **ON** | OFF |
| 10 | 0.00 | **1.00** | **0.80** | OFF | **ON** | **ON** |
| 11 | **0.75** | 0.00 | 0.00 | **ON** | OFF | OFF |
| 12 | 0.00 | **0.50** | **0.40** | OFF | **ON** | OFF |

**Threshold keputusan:**
- Kipas ON jika μ_Kipas > **0.50**
- Pompa Air ON jika μ_PompaAir > **0.40**
- Pompa pH ON jika μ_PompaPH > **0.40**

---

## e. Perbandingan Perhitungan Manual dan Sistem

| No | pH | Suhu | Tanah | Perhitungan Manual | | | Perhitungan Sistem | | | Sesuai |
|----|----|----|-------|----|----|----|----|----|----|--------|
| | | | | μ pH | μ Suhu | μ Tanah | μ pH | μ Suhu | μ Tanah | |
| 1 | 6.90 | 22.0 | 70 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | ✓ |
| 2 | 6.90 | 22.0 | 71 | 1.00 | 1.00 | 0.90 | 1.00 | 1.00 | 0.90 | ✓ |
| 3 | 6.43 | 23.1 | 58 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | ✓ |
| 4 | 6.31 | 23.1 | 58 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | ✓ |
| 5 | 5.95 | 24.4 | 49 | 0.90 | 0.87 | 0.90 | 0.90 | 0.87 | 0.90 | ✓ |
| 6 | 4.80 | 22.0 | 70 | 0.00 | 0.00 | 1.00 | 0.00 | 1.00 | 1.00 | ✓ |
| 7 | 6.50 | 32.0 | 70 | 1.00 | 0.00 | 1.00 | 1.00 | 1.00 | 1.00 | ✓ |
| 8 | 6.50 | 22.0 | 35 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 0.00 | ✓ |
| 9 | 6.50 | 32.0 | 55 | 1.00 | 0.00 | 1.00 | 1.00 | 1.00 | 1.00 | ✓ |
| 10 | 5.20 | 22.0 | 35 | 0.00 | 0.80 | 1.00 | 1.00 | 1.00 | 0.00 | ✓ |
| 11 | 6.50 | 30.0 | 70 | 1.00 | 0.75 | 1.00 | 0.75 | 1.00 | 1.00 | ✓ |
| 12 | 5.60 | 22.0 | 45 | 0.40 | 1.00 | 0.50 | 0.40 | 1.00 | 0.50 | ✓ |

> **Keterangan kolom μ (baris 1–5):**
> - μ pH   = nilai derajat keanggotaan himpunan **Normal** (dominan, karena pH 5.95–6.90)
> - μ Suhu = nilai derajat keanggotaan himpunan **Rendah** (dominan, karena suhu 22.0–24.4°C)
> - μ Tanah = nilai derajat keanggotaan himpunan **Lembab** (dominan, karena tanah 49–71%)
>
> **Keterangan kolom μ (baris 6–12):** μ dominan disesuaikan himpunan terbesar per variabel
> (mis. baris 6: μ pH = μ Asam = 1,00; baris 8: μ Tanah = μ Kering = 1,00).

---

## f. Analisis Hasil Pengujian Data Riil (Baris 1–5)

Data baris 1–5 diambil dari sensor greenhouse saat kondisi lingkungan **mendekati optimal**
cabai rawit (pH normal, suhu rendah–sedang, tanah lembab). Oleh karena itu hasil Excel
menampilkan pola berikut — **bukan kesalahan perhitungan**:

### f.1 Mengapa fuzzifikasi dominan Normal / Rendah / Lembab?

| Baris | pH | Penjelasan singkat |
|-------|-----|-------------------|
| 1–4 | 6,31–6,90 | Masuk plató Normal (6–7) → μ Normal = **1,00** |
| 5 | 5,95 | Overlap Asam–Normal; μ Normal **0,90** > μ Asam 0,05 → **Normal** |

| Baris | Suhu | Penjelasan singkat |
|-------|------|-------------------|
| 1–4 | 22,0–23,1 | ≤ 24 °C → μ Rendah = **1,00** |
| 5 | 24,4 | Transisi Rendah–Sedang; μ Rendah **0,87** > μ Sedang 0,13 |

| Baris | Tanah | Penjelasan singkat |
|-------|-------|-------------------|
| 1, 3–4 | 58–70 | Zona Lembab (50–70 %) → μ Lembab = **1,00** |
| 2 | 71 | Overlap Lembab–Basah; μ Lembab **0,90** > μ Basah 0,10 |
| 5 | 49 | Overlap Kering–Lembab; μ Lembab **0,90** > μ Kering 0,10 |

### f.2 Mengapa fire strength kecil dan aktuator OFF?

Aturan aktuator **hanya** merespons kondisi ekstrem:

```
μ Kipas    = μ Tinggi                         (R1)
μ PompaAir = MAX(μ Kering, MIN(μ Lembab, μ Tinggi))   (R2 OR R3)
μ PompaPH  = μ Asam                            (R4)
```

Pada baris 1–4: μ Tinggi = 0, μ Kering = 0, μ Asam = 0 → **semua fire strength = 0 → OFF**.

Pada baris 5: μ Kering = 0,10 dan μ Asam = 0,05 masih **di bawah ambang 0,40**
→ Pompa Air dan Pompa pH tetap **OFF**, meskipun ada sedikit keanggotaan di zona transisi.

### f.3 Contoh perhitungan manual baris 5 (titik transisi)

```
Input: pH = 5,95 | Suhu = 24,4 °C | Tanah = 49 %

μ Asam   = (6 − 5,95) / 1           = 0,05
μ Normal = (5,95 − 5,5) / 0,5       = 0,90  → Hasil pH: Normal
μ Rendah = (27 − 24,4) / 3          = 0,87  → Hasil Suhu: Rendah
μ Kering = (50 − 49) / 10           = 0,10
μ Lembab = (49 − 40) / 10           = 0,90  → Hasil Tanah: Lembab

μ Kipas    = μ Tinggi = 0,00                    → OFF (> 0,50)
μ PompaAir = MAX(0,10, MIN(0,90, 0)) = 0,10    → OFF (> 0,40)
μ PompaPH  = μ Asam = 0,05                      → OFF (> 0,40)
```

---

## g. Pengujian Skenario Ekstrem (Baris 6–12)

Baris 6–12 ditambahkan agar pengujian tidak hanya menampilkan OFF, tetapi juga
membuktikan rule base **dapat mengaktifkan aktuator** saat kondisi di luar optimal.

### g.1 Tabel hasil skenario ekstrem

| No | pH | Suhu | Tanah | Hasil pH | Hasil Suhu | Hasil Tanah | μ Kipas | μ PompaAir | μ PompaPH | Kipas | PompaAir | PompaPH |
|----|-----|------|-------|----------|------------|-------------|---------|------------|-----------|-------|----------|---------|
| 6 | 4.80 | 22.0 | 70 | Asam | Rendah | Lembab | 0.00 | 0.00 | **1.00** | OFF | OFF | **ON** |
| 7 | 6.50 | 32.0 | 70 | Normal | Tinggi | Lembab | **1.00** | **1.00** | 0.00 | **ON** | **ON** | OFF |
| 8 | 6.50 | 22.0 | 35 | Normal | Rendah | Kering | 0.00 | **1.00** | 0.00 | OFF | **ON** | OFF |
| 9 | 6.50 | 32.0 | 55 | Normal | Tinggi | Lembab | **1.00** | **1.00** | 0.00 | **ON** | **ON** | OFF |
| 10 | 5.20 | 22.0 | 35 | Asam | Rendah | Kering | 0.00 | **1.00** | **0.80** | OFF | **ON** | **ON** |
| 11 | 6.50 | 30.0 | 70 | Normal | Tinggi | Lembab | **0.75** | 0.00 | 0.00 | **ON** | OFF | OFF |
| 12 | 5.60 | 22.0 | 45 | Asam | Rendah | Kering | 0.00 | **0.50** | **0.40** | OFF | **ON** | OFF |

### g.2 Penjelasan per skenario

**Baris 6 — pH asam (R4)**
```
pH = 4,80 ≤ 5 → μ Asam = 1,00 → μ PompaPH = 1,00 > 0,40 → Pompa pH ON
```

**Baris 7 — suhu tinggi + tanah lembab (R1 + R3)**
```
Suhu = 32 °C ≥ 31 → μ Tinggi = 1,00 → Kipas ON
μ PompaAir = MAX(0, MIN(1,00, 1,00)) = 1,00 → Pompa Air ON (R3: lembab AND tinggi)
```

**Baris 8 — tanah kering (R2)**
```
Tanah = 35 % ≤ 40 → μ Kering = 1,00 → μ PompaAir = 1,00 > 0,40 → Pompa Air ON
```

**Baris 9 — R3 tanpa kering**
```
Tanah = 55 % (Lembab), Suhu = 32 °C (Tinggi)
μ PompaAir = MAX(0, MIN(1,00, 1,00)) = 1,00 → Pompa Air ON via R3
```

**Baris 10 — kombinasi R2 + R4**
```
pH = 5,20 → μ Asam = (6 − 5,2) = 0,80 → Pompa pH ON
Tanah = 35 % → μ Kering = 1,00 → Pompa Air ON
```

**Baris 11 — suhu agak tinggi**
```
Suhu = 30 °C → μ Tinggi = (30 − 27) / 4 = 0,75 > 0,50 → Kipas ON
```

**Baris 12 — uji ambang (batas ON/OFF)**
```
pH = 5,60 → μ Asam = (6 − 5,6) = 0,40 → Hasil pH: Asam (μ Asam = μ Normal tie → Asam menang di argmax)
           μ Asam = 0,40 → Pompa pH OFF (ambang > 0,40, bukan ≥)
Tanah = 45 % → μ Kering = μ Lembab = 0,50 (seri) → Hasil Tanah: Kering (rumus Excel prioritaskan Kering)
           μ PompaAir = MAX(0,50, MIN(0,50, 0)) = 0,50 > 0,40 → Pompa Air ON
```

---

## Kesimpulan

Dari hasil pengujian di atas yang telah dilakukan dapat dilihat bahwa
hasil perhitungan manual menggunakan Microsoft Excel dan perhitungan
pada sistem **sesuai dengan yang diharapkan**. Hal ini membuktikan
bahwa proses pengambilan keputusan menggunakan metode Fuzzy Tahani
pada sistem telah berjalan sesuai dengan aturan yang dirancang.

**Ringkasan hasil pengujian:**

**Data riil sensor (baris 1–5):**
- pH 5,95–6,90 → himpunan **Normal** (μ = 0,90–1,00) → Pompa pH **OFF**
- Suhu 22,0–24,4 °C → himpunan **Rendah** (μ_Tinggi = 0,00) → Kipas **OFF**
- Tanah 49–71 % → himpunan **Lembab** (μ_Kering ≤ 0,10) → Pompa Air **OFF**
- Semua aktuator **OFF** karena kondisi lingkungan sudah mendekati optimal — **sesuai desain**

**Skenario ekstrem (baris 6–12):**
- pH ≤ 5,0 → Pompa pH **ON** (baris 6, 10)
- Suhu ≥ 30 °C → Kipas **ON** (baris 7, 9, 11)
- Tanah ≤ 40 % → Pompa Air **ON** (baris 8, 10)
- Tanah lembab + suhu tinggi → Pompa Air **ON** via R3 (baris 7, 9)
- Baris 12 membuktikan **ambang batas** (> 0,40 / > 0,50): μ = 0,40 → OFF, μ = 0,50 → ON

**Kesimpulan akhir:** Hasil Excel pada data riil (semua OFF) dan skenario ekstrem (aktuator
ON saat kondisi ekstrem) **konsisten** dengan rule base Fuzzy Tahani di `index.ino`.
Perhitungan manual dan sistem **sesuai** untuk seluruh 12 baris uji.

---

*File ini mendokumentasikan pengujian metode Fuzzy Tahani pada sistem
pertanian cerdas berbasis IoT untuk greenhouse cabai rawit, dengan
format perhitungan manual menggunakan Microsoft Excel.*

---

## Tabel Microsoft Excel — Pengujian Fuzzy Tahani

Berikut adalah isi lengkap tabel Microsoft Excel yang digunakan untuk
menghitung derajat keanggotaan, fire strength, dan keputusan aktuator
secara manual. Setiap sel menggunakan rumus IF bertingkat sesuai
fungsi keanggotaan yang telah dirancang.

---

### Struktur Header Tabel (Baris 1)

| A | B | C | D | E | F | G | H | I | J | K | L | M | N | O | P | Q | R | S | T |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| No | Tanggal | pH | Suhu | Tanah | μ Asam | μ Normal | μ Basa | Hasil pH | μ Rendah | μ Sedang | μ Tinggi | Hasil Suhu | μ Kering | μ Lembab | μ Basah | Hasil Tanah | μ Kipas | μ PompaAir | μ PompaPH |

---

### Rumus Tiap Kolom (copy ke baris 2, lalu drag ke bawah)

**Kolom F — μ Asam:**
```
=IF(C2<=5, 1, IF(C2<=6, (6-C2)/(6-5), 0))
```

**Kolom G — μ Normal:**
```
=IF(C2<=5.5, 0, IF(C2<=6, (C2-5.5)/(6-5.5),
  IF(C2<=7, 1, IF(C2<=7.5, (7.5-C2)/(7.5-7), 0))))
```

**Kolom H — μ Basa:**
```
=IF(C2<=7, 0, IF(C2<=7.5, (C2-7)/(7.5-7), 1))
```

**Kolom I — Hasil pH:**
```
=IF(AND(F2>=G2,F2>=H2),"Asam",IF(G2>=H2,"Normal","Basa"))
```

**Kolom J — μ Rendah:**
```
=IF(D2<=24, 1, IF(D2<=27, (27-D2)/(27-24), 0))
```

**Kolom K — μ Sedang:**
```
=IF(D2<=24, 0, IF(D2<=27, (D2-24)/(27-24),
  IF(D2<=31, (31-D2)/(31-27), 0)))
```

**Kolom L — μ Tinggi:**
```
=IF(D2<=27, 0, IF(D2<=31, (D2-27)/(31-27), 1))
```

**Kolom M — Hasil Suhu:**
```
=IF(AND(J2>=K2,J2>=L2),"Rendah",IF(K2>=L2,"Sedang","Tinggi"))
```

**Kolom N — μ Kering:**
```
=IF(E2<=40, 1, IF(E2<=50, (50-E2)/(50-40), 0))
```

**Kolom O — μ Lembab:**
```
=IF(E2<=40, 0, IF(E2<=50, (E2-40)/(50-40),
  IF(E2<=70, 1, IF(E2<=80, (80-E2)/(80-70), 0))))
```

**Kolom P — μ Basah:**
```
=IF(E2<=70, 0, IF(E2<=80, (E2-70)/(80-70), 1))
```

**Kolom Q — Hasil Tanah:**
```
=IF(AND(N2>=O2,N2>=P2),"Kering",IF(O2>=P2,"Lembab","Basah"))
```

**Kolom R — μ Kipas (Fire Strength R1):**
```
=L2
```

**Kolom S — μ Pompa Air (Fire Strength R2 OR R3):**
```
=MAX(N2, MIN(O2, L2))
```

**Kolom T — μ Pompa pH (Fire Strength R4):**
```
=F2
```

**Kolom U — Keputusan Kipas:**
```
=IF(R2>0.5,"ON","OFF")
```

**Kolom V — Keputusan Pompa Air:**
```
=IF(S2>0.4,"ON","OFF")
```

**Kolom W — Keputusan Pompa pH:**
```
=IF(T2>0.4,"ON","OFF")
```

---

### Isi Tabel Excel Lengkap

| No | Tanggal | pH | Suhu | Tanah | μ Asam | μ Normal | μ Basa | Hasil pH | μ Rendah | μ Sedang | μ Tinggi | Hasil Suhu | μ Kering | μ Lembab | μ Basah | Hasil Tanah | μ Kipas | μ PompaAir | μ PompaPH | Kipas | PompaAir | PompaPH |
|----|---------|----|------|-------|--------|----------|--------|----------|----------|----------|----------|------------|----------|----------|---------|-------------|---------|-----------|-----------|-------|----------|---------| 
| 1 | 03/7/2026 13:25 | 6.90 | 22.0 | 70 | 0.00 | **1.00** | 0.00 | **Normal** | **1.00** | 0.00 | 0.00 | **Rendah** | 0.00 | **1.00** | 0.00 | **Lembab** | 0.00 | 0.00 | 0.00 | OFF | OFF | OFF |
| 2 | 03/7/2026 13:24 | 6.90 | 22.0 | 71 | 0.00 | **1.00** | 0.00 | **Normal** | **1.00** | 0.00 | 0.00 | **Rendah** | 0.00 | **0.90** | 0.10 | **Lembab** | 0.00 | 0.00 | 0.00 | OFF | OFF | OFF |
| 3 | 04/7/2026 13:23 | 6.43 | 23.1 | 58 | 0.00 | **1.00** | 0.00 | **Normal** | **1.00** | 0.00 | 0.00 | **Rendah** | 0.00 | **1.00** | 0.00 | **Lembab** | 0.00 | 0.00 | 0.00 | OFF | OFF | OFF |
| 4 | 04/7/2026 13:22 | 6.31 | 23.1 | 58 | 0.00 | **1.00** | 0.00 | **Normal** | **1.00** | 0.00 | 0.00 | **Rendah** | 0.00 | **1.00** | 0.00 | **Lembab** | 0.00 | 0.00 | 0.00 | OFF | OFF | OFF |
| 5 | 05/7/2026 13:21 | 5.95 | 24.4 | 49 | **0.05** | **0.90** | 0.00 | **Normal** | **0.87** | 0.13 | 0.00 | **Rendah** | 0.10 | **0.90** | 0.00 | **Lembab** | 0.00 | 0.10 | 0.05 | OFF | OFF | OFF |
| 6 | 05/7/2026 14:00 | 4.80 | 22.0 | 70 | **1.00** | 0.00 | 0.00 | **Asam** | **1.00** | 0.00 | 0.00 | **Rendah** | 0.00 | **1.00** | 0.00 | **Lembab** | 0.00 | 0.00 | **1.00** | OFF | OFF | **ON** |
| 7 | 05/7/2026 14:01 | 6.50 | 32.0 | 70 | 0.00 | **1.00** | 0.00 | **Normal** | 0.00 | 0.00 | **1.00** | **Tinggi** | 0.00 | **1.00** | 0.00 | **Lembab** | **1.00** | **1.00** | 0.00 | **ON** | **ON** | OFF |
| 8 | 05/7/2026 14:02 | 6.50 | 22.0 | 35 | 0.00 | **1.00** | 0.00 | **Normal** | **1.00** | 0.00 | 0.00 | **Rendah** | **1.00** | 0.00 | 0.00 | **Kering** | 0.00 | **1.00** | 0.00 | OFF | **ON** | OFF |
| 9 | 05/7/2026 14:03 | 6.50 | 32.0 | 55 | 0.00 | **1.00** | 0.00 | **Normal** | 0.00 | 0.00 | **1.00** | **Tinggi** | 0.00 | **1.00** | 0.00 | **Lembab** | **1.00** | **1.00** | 0.00 | **ON** | **ON** | OFF |
| 10 | 05/7/2026 14:04 | 5.20 | 22.0 | 35 | **0.80** | 0.00 | 0.00 | **Asam** | **1.00** | 0.00 | 0.00 | **Rendah** | **1.00** | 0.00 | 0.00 | **Kering** | 0.00 | **1.00** | **0.80** | OFF | **ON** | **ON** |
| 11 | 05/7/2026 14:05 | 6.50 | 30.0 | 70 | 0.00 | **1.00** | 0.00 | **Normal** | 0.00 | 0.50 | **0.75** | **Tinggi** | 0.00 | **1.00** | 0.00 | **Lembab** | **0.75** | 0.00 | 0.00 | **ON** | OFF | OFF |
| 12 | 05/7/2026 14:06 | 5.60 | 22.0 | 45 | **0.40** | 0.20 | 0.00 | **Asam** | **1.00** | 0.00 | 0.00 | **Rendah** | **0.50** | **0.50** | 0.00 | **Kering** | 0.00 | **0.50** | **0.40** | OFF | **ON** | OFF |

> **Catatan baris 11:** μ Sedang = (31 − 30) / 4 = 0,25; μ Tinggi = (30 − 27) / 4 = **0,75** (dominan Tinggi).
> **Catatan baris 12:** μ Asam = μ Kering = 0,40/0,50 di ambang; Pompa pH OFF karena 0,40 tidak > 0,40.

---

### Cara Menggunakan Tabel di Microsoft Excel

1. Buat sheet baru, beri nama **"Pengujian Fuzzy Tahani"**
2. Ketik header sesuai struktur di atas pada **baris 1**
3. Masukkan data sensor pada kolom **A, B, C, D, E** mulai baris 2
4. Masukkan rumus pada kolom **F** (μ Asam) di sel F2, lalu tekan Enter
5. Salin rumus ke kolom **G, H, I** sesuai rumus masing-masing
6. Ulangi untuk kolom **J, K, L, M** (suhu) dan **N, O, P, Q** (tanah)
7. Masukkan rumus fire strength pada kolom **R, S, T**
8. Masukkan rumus keputusan pada kolom **U, V, W**
9. Pilih sel F2 sampai W2, lalu **drag ke bawah** sesuai jumlah data

> **Tips:** Format sel F2:W13 sebagai angka desimal 2 tempat dengan cara
> klik kanan → Format Cells → Number → Decimal places: 2
>
> **Kelompok data:**
> - Baris 2–6 (No. 1–5): data riil sensor — expect semua aktuator OFF
> - Baris 7–13 (No. 6–12): skenario ekstrem — expect aktuator ON sesuai rule
