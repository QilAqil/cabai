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
| pH Tanah | Asam   | 3   | 5   | 6   |
| pH Tanah | Normal | 5.5 | 6.5 | 7.5 |
| pH Tanah | Basa   | 7   | 7.5 | 9   |
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

**Kelompok A — Data riil sensor:**

| No | Tanggal          | pH Tanah | Suhu (°C) | Kelembapan Tanah (%) |
|----|------------------|----------|-----------|----------------------|
| 1  | 03/7/2026 13:25  | 6.90     | 22.0      | 70                   |
| 2  | 03/7/2026 13:24  | 6.90     | 22.0      | 71                   |
| 3  | 04/7/2026 13:23  | 6.43     | 23.1      | 58                   |
| 4  | 04/7/2026 13:22  | 6.31     | 23.1      | 58                   |
| 5  | 05/7/2026 13:21  | 5.95     | 24.4      | 49                   |

**Kelompok B — Skenario ekstrem (simulasi / uji rule base):**

| No | Tanggal          | pH Tanah | Suhu (°C) | Kelembapan Tanah (%) | Tujuan Uji                                        |
|----|------------------|----------|-----------|----------------------|---------------------------------------------------|
| 6  | 05/7/2026 14:00  | 4.80     | 22.0      | 70                   | R3 — pH asam → Pompa pH ON                        |
| 7  | 05/7/2026 14:01  | 6.50     | 32.0      | 70                   | R1 — suhu tinggi → Kipas ON                       |
| 8  | 05/7/2026 14:02  | 6.50     | 22.0      | 35                   | R2 — tanah kering → Pompa Air ON                  |
| 9  | 05/7/2026 14:03  | 5.20     | 22.0      | 35                   | R2 + R3 → Pompa Air & Pompa pH ON                 |
| 10 | 05/7/2026 14:04  | 6.50     | 30.0      | 70                   | R1 — suhu agak tinggi (μ Tinggi = 0,75)           |
| 11 | 05/7/2026 14:05  | 5.60     | 22.0      | 45                   | Ambang — μ Asam = 0,40 (OFF), μ Kering = 0,50 (ON)|

---

## Pengujian Manual Menggunakan Microsoft Excel

Perhitungan dilakukan menggunakan Microsoft Excel dengan cara
memasukkan rumus IF bertingkat pada setiap kolom derajat keanggotaan.
Struktur kolom pada Excel adalah sebagai berikut:

```
Kolom A  = No
Kolom B  = Tanggal
Kolom C  = pH Tanah
Kolom D  = Suhu Udara (°C)
Kolom E  = Kelembapan Tanah (%)
Kolom F  = μ Asam
Kolom G  = μ Normal
Kolom H  = μ Basa
Kolom I  = Hasil pH
Kolom J  = μ Rendah
Kolom K  = μ Sedang
Kolom L  = μ Tinggi
Kolom M  = Hasil Suhu
Kolom N  = μ Kering
Kolom O  = μ Lembab
Kolom P  = μ Basah
Kolom Q  = Hasil Tanah
Kolom R  = μ Kipas      (Fire Strength R1)
Kolom S  = μ Pompa Air  (Fire Strength R2)
Kolom T  = μ Pompa pH   (Fire Strength R3)
Kolom U  = Keputusan Kipas
Kolom V  = Keputusan Pompa Air
Kolom W  = Keputusan Pompa pH
```

---

## a. Fuzzifikasi pada Nilai pH Tanah

➢ Nilai keanggotaan Asam
```excel
=IF(C2<=5, 1, IF(C2<=6, (6-C2)/(6-5), 0))
```

➢ Nilai keanggotaan Normal
```excel
=IF(C2<=5.5, 0, IF(C2<=6, (C2-5.5)/(6-5.5), IF(C2<=7, 1, IF(C2<=7.5, (7.5-C2)/(7.5-7), 0))))
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

| No | pH   | μ Asam | μ Normal | μ Basa | Hasil pH |
|----|------|--------|----------|--------|----------|
| 1  | 6.90 | 0.00   | 1.00     | 0.00   | Normal   |
| 2  | 6.90 | 0.00   | 1.00     | 0.00   | Normal   |
| 3  | 6.43 | 0.00   | 1.00     | 0.00   | Normal   |
| 4  | 6.31 | 0.00   | 1.00     | 0.00   | Normal   |
| 5  | 5.95 | 0.05   | 0.90     | 0.00   | Normal   |

**Cara menghitung manual (No. 1, pH = 6.90):**
```
μ Asam   : 6.90 > 6              → 0.00
μ Normal : 6 < 6.90 ≤ 7         → 1.00
μ Basa   : 6.90 ≤ 7             → 0.00
Hasil    : argmax(0.00, 1.00, 0.00) = Normal
```

**Cara menghitung manual (No. 5, pH = 5.95):**
```
μ Asam   : 5 < 5.95 ≤ 6  → (6 − 5.95) / 1 = 0.05
μ Normal : 5.5 < 5.95 ≤ 6 → (5.95 − 5.5) / 0.5 = 0.90
μ Basa   : 5.95 ≤ 7       → 0.00
Hasil    : argmax(0.05, 0.90, 0.00) = Normal
```

---

## b. Fuzzifikasi pada Nilai Suhu Udara

➢ Nilai keanggotaan Rendah
```excel
=IF(D2<=24, 1, IF(D2<=27, (27-D2)/(27-24), 0))
```

➢ Nilai keanggotaan Sedang
```excel
=IF(D2<=24, 0, IF(D2<=27, (D2-24)/(27-24), IF(D2<=31, (31-D2)/(31-27), 0)))
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

| No | Suhu | μ Rendah | μ Sedang | μ Tinggi | Hasil Suhu |
|----|------|----------|----------|----------|------------|
| 1  | 22.0 | 1.00     | 0.00     | 0.00     | Rendah     |
| 2  | 22.0 | 1.00     | 0.00     | 0.00     | Rendah     |
| 3  | 23.1 | 1.00     | 0.00     | 0.00     | Rendah     |
| 4  | 23.1 | 1.00     | 0.00     | 0.00     | Rendah     |
| 5  | 24.4 | 0.87     | 0.13     | 0.00     | Rendah     |

**Cara menghitung manual (No. 1, Suhu = 22.0°C):**
```
μ Rendah : 22.0 ≤ 24  → 1.00
μ Sedang : 22.0 ≤ 24  → 0.00
μ Tinggi : 22.0 ≤ 27  → 0.00
Hasil    : argmax(1.00, 0.00, 0.00) = Rendah
```

**Cara menghitung manual (No. 5, Suhu = 24.4°C):**
```
μ Rendah : 24 < 24.4 ≤ 27 → (27 − 24.4) / 3 = 2.6 / 3 = 0.87
μ Sedang : 24 < 24.4 ≤ 27 → (24.4 − 24) / 3 = 0.4 / 3 = 0.13
μ Tinggi : 24.4 ≤ 27       → 0.00
Hasil    : argmax(0.87, 0.13, 0.00) = Rendah
```

---

## c. Fuzzifikasi pada Nilai Kelembapan Tanah

➢ Nilai keanggotaan Kering
```excel
=IF(E2<=40, 1, IF(E2<=50, (50-E2)/(50-40), 0))
```

➢ Nilai keanggotaan Lembab
```excel
=IF(E2<=40, 0, IF(E2<=50, (E2-40)/(50-40), IF(E2<=70, 1, IF(E2<=80, (80-E2)/(80-70), 0))))
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

| No | Tanah (%) | μ Kering | μ Lembab | μ Basah | Hasil Tanah |
|----|-----------|----------|----------|---------|-------------|
| 1  | 70        | 0.00     | 1.00     | 0.00    | Lembab      |
| 2  | 71        | 0.00     | 0.90     | 0.10    | Lembab      |
| 3  | 58        | 0.00     | 1.00     | 0.00    | Lembab      |
| 4  | 58        | 0.00     | 1.00     | 0.00    | Lembab      |
| 5  | 49        | 0.10     | 0.90     | 0.00    | Lembab      |

**Cara menghitung manual (No. 1, Tanah = 70%):**
```
μ Kering : 70 > 50              → 0.00
μ Lembab : 50 < 70 ≤ 70         → 1.00
μ Basah  : 70 ≤ 70              → 0.00
Hasil    : argmax(0.00, 1.00, 0.00) = Lembab
```

**Cara menghitung manual (No. 2, Tanah = 71%):**
```
μ Kering : 71 > 50              → 0.00
μ Lembab : 70 < 71 ≤ 80  → (80 − 71) / 10 = 0.90
μ Basah  : 70 < 71 ≤ 80  → (71 − 70) / 10 = 0.10
Hasil    : argmax(0.00, 0.90, 0.10) = Lembab
```

**Cara menghitung manual (No. 5, Tanah = 49%):**
```
μ Kering : 40 < 49 ≤ 50  → (50 − 49) / 10 = 0.10
μ Lembab : 40 < 49 ≤ 50  → (49 − 40) / 10 = 0.90
μ Basah  : 49 ≤ 70               → 0.00
Hasil    : argmax(0.10, 0.90, 0.00) = Lembab
```

---

## d. Pembentukan Query Fuzzy dan Fire Strength

Setelah fuzzifikasi, sistem membentuk query fuzzy berdasarkan
rule base yang telah dirancang:

| No | Rule | Kondisi        | Operator | Fire Strength | Aktuator  |
|----|------|----------------|----------|---------------|-----------|
| R1 | IF suhu Tinggi  | μ Tinggi | —        | μ Tinggi      | KIPAS     |
| R2 | IF tanah Kering | μ Kering | —        | μ Kering      | POMPA AIR |
| R3 | IF pH Asam      | μ Asam   | —        | μ Asam        | POMPA pH  |

**Rumus Excel untuk Fire Strength:**
```excel
Kolom R — μ Kipas    : =L2    (μ Tinggi)
Kolom S — μ Pompa Air: =N2    (μ Kering)
Kolom T — μ Pompa pH : =F2    (μ Asam)
```

**Rumus Excel untuk Keputusan Aktuator:**
```excel
Kolom U — Kipas    : =IF(R2>0.5,"ON","OFF")
Kolom V — Pompa Air: =IF(S2>0.4,"ON","OFF")
Kolom W — Pompa pH : =IF(T2>0.4,"ON","OFF")
```

**Threshold keputusan:**
- Kipas ON jika μ Kipas > **0.50**
- Pompa Air ON jika μ Pompa Air > **0.40**
- Pompa pH ON jika μ Pompa pH > **0.40**

### Tabel Fire Strength dan Keputusan Aktuator

| No | μ Kipas | μ Pompa Air | μ Pompa pH | Kipas | Pompa Air | Pompa pH |
|----|---------|-------------|------------|-------|-----------|----------|
| 1  | 0.00    | 0.00        | 0.00       | OFF   | OFF       | OFF      |
| 2  | 0.00    | 0.00        | 0.00       | OFF   | OFF       | OFF      |
| 3  | 0.00    | 0.00        | 0.00       | OFF   | OFF       | OFF      |
| 4  | 0.00    | 0.00        | 0.00       | OFF   | OFF       | OFF      |
| 5  | 0.00    | 0.10        | 0.05       | OFF   | OFF       | OFF      |
| 6  | 0.00    | 0.00        | 1.00       | OFF   | OFF       | **ON**   |
| 7  | 1.00    | 0.00        | 0.00       | **ON**| OFF       | OFF      |
| 8  | 0.00    | 1.00        | 0.00       | OFF   | **ON**    | OFF      |
| 9  | 0.00    | 1.00        | 0.80       | OFF   | **ON**    | **ON**   |
| 10 | 0.75    | 0.00        | 0.00       | **ON**| OFF       | OFF      |
| 11 | 0.00    | 0.50        | 0.40       | OFF   | **ON**    | OFF      |

---

## e. Perbandingan Perhitungan Manual dan Sistem

| No | pH   | Suhu | Tanah | μ pH (Manual) | μ Suhu (Manual) | μ Tanah (Manual) | μ pH (Sistem) | μ Suhu (Sistem) | μ Tanah (Sistem) | Sesuai |
|----|------|------|-------|---------------|-----------------|------------------|---------------|-----------------|------------------|--------|
| 1  | 6.90 | 22.0 | 70    | 1.00          | 1.00            | 1.00             | 1.00          | 1.00            | 1.00             | ✓      |
| 2  | 6.90 | 22.0 | 71    | 1.00          | 1.00            | 0.90             | 1.00          | 1.00            | 0.90             | ✓      |
| 3  | 6.43 | 23.1 | 58    | 1.00          | 1.00            | 1.00             | 1.00          | 1.00            | 1.00             | ✓      |
| 4  | 6.31 | 23.1 | 58    | 1.00          | 1.00            | 1.00             | 1.00          | 1.00            | 1.00             | ✓      |
| 5  | 5.95 | 24.4 | 49    | 0.90          | 0.87            | 0.90             | 0.90          | 0.87            | 0.90             | ✓      |
| 6  | 4.80 | 22.0 | 70    | 1.00          | 1.00            | 1.00             | 1.00          | 1.00            | 1.00             | ✓      |
| 7  | 6.50 | 32.0 | 70    | 1.00          | 1.00            | 1.00             | 1.00          | 1.00            | 1.00             | ✓      |
| 8  | 6.50 | 22.0 | 35    | 1.00          | 1.00            | 1.00             | 1.00          | 1.00            | 1.00             | ✓      |
| 9  | 5.20 | 22.0 | 35    | 0.80          | 1.00            | 1.00             | 0.80          | 1.00            | 1.00             | ✓      |
| 10 | 6.50 | 30.0 | 70    | 1.00          | 0.75            | 1.00             | 1.00          | 0.75            | 1.00             | ✓      |
| 11 | 5.60 | 22.0 | 45    | 0.40          | 1.00            | 0.50             | 0.40          | 1.00            | 0.50             | ✓      |

> **Keterangan kolom μ:**  
> Nilai yang ditampilkan adalah derajat keanggotaan **himpunan dominan** per variabel.  
> - μ pH   = μ Normal (baris 1–5, 7–8, 10–11), μ Asam (baris 6, 9)  
> - μ Suhu = μ Rendah (baris 1–5, 6, 8–9, 11), μ Tinggi (baris 7, 10)  
> - μ Tanah = μ Lembab (baris 1–4, 6–7, 10), μ Kering (baris 8–9, 11), μ Lembab (baris 5)

---

## f. Analisis Hasil Pengujian Data Riil (Baris 1–5)

Data baris 1–5 diambil dari sensor saat kondisi lingkungan **mendekati optimal**
untuk tanaman cabai rawit (pH normal, suhu rendah–sedang, tanah lembab).
Seluruh aktuator OFF — **bukan kesalahan perhitungan**, melainkan sesuai desain sistem.

### f.1 Mengapa fuzzifikasi dominan Normal / Rendah / Lembab?

| Baris | pH       | Penjelasan                                             |
|-------|----------|--------------------------------------------------------|
| 1–4   | 6.31–6.90 | Masuk plató Normal (6–7) → μ Normal = 1.00            |
| 5     | 5.95      | Transisi Asam–Normal; μ Normal 0.90 > μ Asam 0.05    |

| Baris | Suhu (°C) | Penjelasan                                             |
|-------|-----------|--------------------------------------------------------|
| 1–4   | 22.0–23.1 | ≤ 24 °C → μ Rendah = 1.00                             |
| 5     | 24.4      | Transisi Rendah–Sedang; μ Rendah 0.87 > μ Sedang 0.13|

| Baris | Tanah (%) | Penjelasan                                             |
|-------|-----------|--------------------------------------------------------|
| 1, 3–4| 58–70    | Zona Lembab (50–70 %) → μ Lembab = 1.00               |
| 2     | 71        | Transisi Lembab–Basah; μ Lembab 0.90 > μ Basah 0.10  |
| 5     | 49        | Transisi Kering–Lembab; μ Lembab 0.90 > μ Kering 0.10|

### f.2 Mengapa semua aktuator OFF?

Aturan hanya merespons kondisi ekstrem:

```
μ Kipas    = μ Tinggi    (R1)
μ Pompa Air = μ Kering   (R2)
μ Pompa pH  = μ Asam     (R3)
```

Pada baris 1–4: μ Tinggi = 0, μ Kering = 0, μ Asam = 0 → fire strength = 0 → semua OFF.

Pada baris 5: μ Kering = 0.10 dan μ Asam = 0.05, keduanya di bawah ambang 0.40
→ Pompa Air dan Pompa pH tetap OFF.

### f.3 Contoh perhitungan manual baris 5 (titik transisi)

```
Input: pH = 5.95 | Suhu = 24.4°C | Tanah = 49%

μ Asam   = (6 − 5.95) / 1      = 0.05
μ Normal = (5.95 − 5.5) / 0.5  = 0.90  → Hasil pH   : Normal
μ Rendah = (27 − 24.4) / 3     = 0.87  → Hasil Suhu  : Rendah
μ Kering = (50 − 49) / 10      = 0.10
μ Lembab = (49 − 40) / 10      = 0.90  → Hasil Tanah : Lembab

μ Kipas    = μ Tinggi = 0.00           → OFF (threshold > 0.50)
μ Pompa Air = μ Kering = 0.10          → OFF (threshold > 0.40)
μ Pompa pH  = μ Asam  = 0.05          → OFF (threshold > 0.40)
```

---

## g. Pengujian Skenario Ekstrem (Baris 6–11)

Baris 6–11 ditambahkan untuk membuktikan rule base **dapat mengaktifkan aktuator**
saat kondisi lingkungan di luar rentang optimal.

### g.1 Tabel hasil skenario ekstrem

| No | pH   | Suhu | Tanah | Hasil pH | Hasil Suhu | Hasil Tanah | μ Kipas | μ Pompa Air | μ Pompa pH | Kipas  | Pompa Air | Pompa pH |
|----|------|------|-------|----------|------------|-------------|---------|-------------|------------|--------|-----------|----------|
| 6  | 4.80 | 22.0 | 70    | Asam     | Rendah     | Lembab      | 0.00    | 0.00        | 1.00       | OFF    | OFF       | **ON**   |
| 7  | 6.50 | 32.0 | 70    | Normal   | Tinggi     | Lembab      | 1.00    | 0.00        | 0.00       | **ON** | OFF       | OFF      |
| 8  | 6.50 | 22.0 | 35    | Normal   | Rendah     | Kering      | 0.00    | 1.00        | 0.00       | OFF    | **ON**    | OFF      |
| 9  | 5.20 | 22.0 | 35    | Asam     | Rendah     | Kering      | 0.00    | 1.00        | 0.80       | OFF    | **ON**    | **ON**   |
| 10 | 6.50 | 30.0 | 70    | Normal   | Tinggi     | Lembab      | 0.75    | 0.00        | 0.00       | **ON** | OFF       | OFF      |
| 11 | 5.60 | 22.0 | 45    | Asam     | Rendah     | Kering      | 0.00    | 0.50        | 0.40       | OFF    | **ON**    | OFF      |

### g.2 Penjelasan per skenario

**Baris 6 — pH asam (R3)**
```
pH = 4.80 ≤ 5  → μ Asam = 1.00 > 0.40  → Pompa pH ON
```

**Baris 7 — suhu tinggi (R1)**
```
Suhu = 32°C ≥ 31  → μ Tinggi = 1.00 > 0.50  → Kipas ON
Tanah = 70% (Lembab)  → μ Kering = 0.00  → Pompa Air OFF
```

**Baris 8 — tanah kering (R2)**
```
Tanah = 35% ≤ 40  → μ Kering = 1.00 > 0.40  → Pompa Air ON
```

**Baris 9 — kombinasi R2 + R3**
```
Tanah = 35% ≤ 40  → μ Kering = 1.00 > 0.40  → Pompa Air ON
pH = 5.20  → μ Asam = (6 − 5.2) / 1 = 0.80 > 0.40  → Pompa pH ON
```

**Baris 10 — suhu agak tinggi (R1, μ parsial)**
```
Suhu = 30°C  → μ Tinggi = (30 − 27) / 4 = 0.75 > 0.50  → Kipas ON
```

**Baris 11 — uji ambang batas ON/OFF**
```
pH = 5.60  → μ Asam = (6 − 5.6) / 1 = 0.40
             0.40 tidak > 0.40  → Pompa pH OFF  (ambang ketat, bukan ≥)

Tanah = 45%  → μ Kering = (50 − 45) / 10 = 0.50
              0.50 > 0.40  → Pompa Air ON
```

---

## Kesimpulan

Dari hasil pengujian yang telah dilakukan dapat dilihat bahwa
hasil perhitungan manual menggunakan Microsoft Excel dan perhitungan
pada sistem **sesuai dengan yang diharapkan**. Hal ini membuktikan
bahwa proses pengambilan keputusan menggunakan metode Fuzzy Tahani
pada sistem telah berjalan sesuai dengan aturan yang dirancang.

**Ringkasan hasil pengujian:**

**Data riil sensor (baris 1–5):**
- pH 5.95–6.90 → himpunan **Normal** (μ = 0.90–1.00) → Pompa pH **OFF**
- Suhu 22.0–24.4°C → himpunan **Rendah** (μ Tinggi = 0.00) → Kipas **OFF**
- Tanah 49–71% → himpunan **Lembab** (μ Kering ≤ 0.10) → Pompa Air **OFF**
- Semua aktuator **OFF** — kondisi mendekati optimal, sesuai desain

**Skenario ekstrem (baris 6–11):**
- pH ≤ 5.0 → Pompa pH **ON** (baris 6, 9)
- Suhu ≥ 30°C → Kipas **ON** (baris 7, 10)
- Tanah ≤ 40% → Pompa Air **ON** (baris 8, 9)
- Baris 11 membuktikan ambang batas: μ = 0.40 → OFF, μ = 0.50 → ON

**Kesimpulan akhir:** Hasil perhitungan manual dan sistem **konsisten** untuk
seluruh 11 baris uji, membuktikan implementasi Fuzzy Tahani di `index.ino`
berjalan sesuai rule base yang dirancang.

---

*File ini mendokumentasikan pengujian metode Fuzzy Tahani pada sistem
pertanian cerdas berbasis IoT untuk greenhouse cabai rawit.*

---

## Lampiran — Tabel Lengkap Microsoft Excel

### Struktur Header (Baris 1)

| A  | B       | C  | D    | E     | F      | G        | H      | I        | J        | K        | L        | M          | N        | O        | P        | Q           | R       | S          | T        | U     | V         | W        |
|----|---------|----|------|-------|--------|----------|--------|----------|----------|----------|----------|------------|----------|----------|----------|-------------|---------|------------|----------|-------|-----------|----------|
| No | Tanggal | pH | Suhu | Tanah | μ Asam | μ Normal | μ Basa | Hasil pH | μ Rendah | μ Sedang | μ Tinggi | Hasil Suhu | μ Kering | μ Lembab | μ Basah | Hasil Tanah | μ Kipas | μ PompaAir | μ PompaPH | Kipas | PompaAir | PompaPH |

### Rumus Tiap Kolom (masukkan di baris 2, drag ke bawah)

| Kolom | Nama        | Rumus Excel |
|-------|-------------|-------------|
| F     | μ Asam      | `=IF(C2<=5,1,IF(C2<=6,(6-C2)/(6-5),0))` |
| G     | μ Normal    | `=IF(C2<=5.5,0,IF(C2<=6,(C2-5.5)/(6-5.5),IF(C2<=7,1,IF(C2<=7.5,(7.5-C2)/(7.5-7),0))))` |
| H     | μ Basa      | `=IF(C2<=7,0,IF(C2<=7.5,(C2-7)/(7.5-7),1))` |
| I     | Hasil pH    | `=IF(AND(F2>=G2,F2>=H2),"Asam",IF(G2>=H2,"Normal","Basa"))` |
| J     | μ Rendah    | `=IF(D2<=24,1,IF(D2<=27,(27-D2)/(27-24),0))` |
| K     | μ Sedang    | `=IF(D2<=24,0,IF(D2<=27,(D2-24)/(27-24),IF(D2<=31,(31-D2)/(31-27),0)))` |
| L     | μ Tinggi    | `=IF(D2<=27,0,IF(D2<=31,(D2-27)/(31-27),1))` |
| M     | Hasil Suhu  | `=IF(AND(J2>=K2,J2>=L2),"Rendah",IF(K2>=L2,"Sedang","Tinggi"))` |
| N     | μ Kering    | `=IF(E2<=40,1,IF(E2<=50,(50-E2)/(50-40),0))` |
| O     | μ Lembab    | `=IF(E2<=40,0,IF(E2<=50,(E2-40)/(50-40),IF(E2<=70,1,IF(E2<=80,(80-E2)/(80-70),0))))` |
| P     | μ Basah     | `=IF(E2<=70,0,IF(E2<=80,(E2-70)/(80-70),1))` |
| Q     | Hasil Tanah | `=IF(AND(N2>=O2,N2>=P2),"Kering",IF(O2>=P2,"Lembab","Basah"))` |
| R     | μ Kipas     | `=L2` |
| S     | μ PompaAir  | `=N2` |
| T     | μ PompaPH   | `=F2` |
| U     | Kipas       | `=IF(R2>0.5,"ON","OFF")` |
| V     | PompaAir    | `=IF(S2>0.4,"ON","OFF")` |
| W     | PompaPH     | `=IF(T2>0.4,"ON","OFF")` |

### Isi Tabel Excel Lengkap

| No | Tanggal         | pH   | Suhu | Tanah | μ Asam | μ Normal | μ Basa | Hasil pH | μ Rendah | μ Sedang | μ Tinggi | Hasil Suhu | μ Kering | μ Lembab | μ Basah | Hasil Tanah | μ Kipas | μ PompaAir | μ PompaPH | Kipas  | PompaAir | PompaPH |
|----|-----------------|------|------|-------|--------|----------|--------|----------|----------|----------|----------|------------|----------|----------|---------|-------------|---------|------------|-----------|--------|----------|---------|
| 1  | 03/7/2026 13:25 | 6.90 | 22.0 | 70    | 0.00   | 1.00     | 0.00   | Normal   | 1.00     | 0.00     | 0.00     | Rendah     | 0.00     | 1.00     | 0.00    | Lembab      | 0.00    | 0.00       | 0.00      | OFF    | OFF      | OFF     |
| 2  | 03/7/2026 13:24 | 6.90 | 22.0 | 71    | 0.00   | 1.00     | 0.00   | Normal   | 1.00     | 0.00     | 0.00     | Rendah     | 0.00     | 0.90     | 0.10    | Lembab      | 0.00    | 0.00       | 0.00      | OFF    | OFF      | OFF     |
| 3  | 04/7/2026 13:23 | 6.43 | 23.1 | 58    | 0.00   | 1.00     | 0.00   | Normal   | 1.00     | 0.00     | 0.00     | Rendah     | 0.00     | 1.00     | 0.00    | Lembab      | 0.00    | 0.00       | 0.00      | OFF    | OFF      | OFF     |
| 4  | 04/7/2026 13:22 | 6.31 | 23.1 | 58    | 0.00   | 1.00     | 0.00   | Normal   | 1.00     | 0.00     | 0.00     | Rendah     | 0.00     | 1.00     | 0.00    | Lembab      | 0.00    | 0.00       | 0.00      | OFF    | OFF      | OFF     |
| 5  | 05/7/2026 13:21 | 5.95 | 24.4 | 49    | 0.05   | 0.90     | 0.00   | Normal   | 0.87     | 0.13     | 0.00     | Rendah     | 0.10     | 0.90     | 0.00    | Lembab      | 0.00    | 0.10       | 0.05      | OFF    | OFF      | OFF     |
| 6  | 05/7/2026 14:00 | 4.80 | 22.0 | 70    | 1.00   | 0.00     | 0.00   | Asam     | 1.00     | 0.00     | 0.00     | Rendah     | 0.00     | 1.00     | 0.00    | Lembab      | 0.00    | 0.00       | 1.00      | OFF    | OFF      | **ON**  |
| 7  | 05/7/2026 14:01 | 6.50 | 32.0 | 70    | 0.00   | 1.00     | 0.00   | Normal   | 0.00     | 0.00     | 1.00     | Tinggi     | 0.00     | 1.00     | 0.00    | Lembab      | 1.00    | 0.00       | 0.00      | **ON** | OFF      | OFF     |
| 8  | 05/7/2026 14:02 | 6.50 | 22.0 | 35    | 0.00   | 1.00     | 0.00   | Normal   | 1.00     | 0.00     | 0.00     | Rendah     | 1.00     | 0.00     | 0.00    | Kering      | 0.00    | 1.00       | 0.00      | OFF    | **ON**   | OFF     |
| 9  | 05/7/2026 14:03 | 5.20 | 22.0 | 35    | 0.80   | 0.00     | 0.00   | Asam     | 1.00     | 0.00     | 0.00     | Rendah     | 1.00     | 0.00     | 0.00    | Kering      | 0.00    | 1.00       | 0.80      | OFF    | **ON**   | **ON**  |
| 10 | 05/7/2026 14:04 | 6.50 | 30.0 | 70    | 0.00   | 1.00     | 0.00   | Normal   | 0.00     | 0.25     | 0.75     | Tinggi     | 0.00     | 1.00     | 0.00    | Lembab      | 0.75    | 0.00       | 0.00      | **ON** | OFF      | OFF     |
| 11 | 05/7/2026 14:05 | 5.60 | 22.0 | 45    | 0.40   | 0.20     | 0.00   | Asam     | 1.00     | 0.00     | 0.00     | Rendah     | 0.50     | 0.50     | 0.00    | Kering      | 0.00    | 0.50       | 0.40      | OFF    | **ON**   | OFF     |

> **Catatan baris 10:** μ Sedang = (31 − 30) / 4 = 0.25 ; μ Tinggi = (30 − 27) / 4 = **0.75** → Hasil Suhu: Tinggi  
> **Catatan baris 11:** μ Asam = 0.40 → Pompa pH **OFF** (ambang **>** 0.40, bukan ≥) ; μ Kering = 0.50 → Pompa Air **ON**

### Cara Menggunakan di Microsoft Excel

1. Buat sheet baru, beri nama **"Pengujian Fuzzy Tahani"**
2. Ketik header sesuai struktur pada **baris 1**
3. Masukkan data sensor (kolom A–E) mulai baris 2
4. Masukkan rumus fuzzifikasi pada kolom F–Q sesuai tabel rumus di atas
5. Masukkan rumus fire strength pada kolom R, S, T
6. Masukkan rumus keputusan pada kolom U, V, W
7. Pilih sel F2 sampai W2, drag ke bawah sesuai jumlah data
8. Format kolom F–T sebagai angka desimal 2 tempat: klik kanan → Format Cells → Number → Decimal places: 2

> **Kelompok data:**  
> Baris 2–6 (No. 1–5): data riil sensor — semua aktuator expected OFF  
> Baris 7–12 (No. 6–11): skenario ekstrem — aktuator ON sesuai rule yang diuji
