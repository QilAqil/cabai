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
| 1  | 25/6/2026 13:25 | 5.0 | 27.4 | 79 |
| 2  | 25/6/2026 13:24 | 5.0 | 27.5 | 79 |
| 3  | 25/6/2026 13:23 | 5.0 | 27.5 | 78 |
| 4  | 25/6/2026 13:22 | 5.0 | 27.4 | 79 |
| 5  | 25/6/2026 13:21 | 5.0 | 27.2 | 79 |

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
| 1 | 5.0 | IF(5.0≤5,1,...) = **1.00** | 0.00 | 0.00 | **Asam** |
| 2 | 5.0 | **1.00** | 0.00 | 0.00 | **Asam** |
| 3 | 5.0 | **1.00** | 0.00 | 0.00 | **Asam** |
| 4 | 5.0 | **1.00** | 0.00 | 0.00 | **Asam** |
| 5 | 5.0 | **1.00** | 0.00 | 0.00 | **Asam** |

**Cara menghitung manual (No. 1, pH = 5.0):**
```
μ_Asam   : pH=5.0 ≤ 5 → μ = 1.00 ✓
μ_Normal : pH=5.0 ≤ 5.5 → μ = 0.00
μ_Basa   : pH=5.0 ≤ 7   → μ = 0.00
Hasil    : argmax(1.00, 0.00, 0.00) = Asam
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
| 1 | 27.4 | 0.00 | (31-27.4)/4 = **0.90** | (27.4-27)/4 = **0.10** | **Sedang** |
| 2 | 27.5 | 0.00 | (31-27.5)/4 = **0.88** | (27.5-27)/4 = **0.13** | **Sedang** |
| 3 | 27.5 | 0.00 | **0.88** | **0.13** | **Sedang** |
| 4 | 27.4 | 0.00 | **0.90** | **0.10** | **Sedang** |
| 5 | 27.2 | 0.00 | (31-27.2)/4 = **0.95** | (27.2-27)/4 = **0.05** | **Sedang** |

**Cara menghitung manual (No. 1, Suhu = 27.4°C):**
```
μ_Rendah : 27.4 > 27 → μ = 0.00
μ_Sedang : 27 < 27.4 < 31 → μ = (31 - 27.4) / (31 - 27) = 3.6 / 4 = 0.90 ✓
μ_Tinggi : 27 < 27.4 < 31 → μ = (27.4 - 27) / (31 - 27) = 0.4 / 4 = 0.10
Hasil    : argmax(0.00, 0.90, 0.10) = Sedang
```---

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
| 1 | 79 | 0.00 | (80-79)/10 = **0.10** | (79-70)/10 = **0.90** | **Basah** |
| 2 | 79 | 0.00 | **0.10** | **0.90** | **Basah** |
| 3 | 78 | 0.00 | (80-78)/10 = **0.20** | (78-70)/10 = **0.80** | **Basah** |
| 4 | 79 | 0.00 | **0.10** | **0.90** | **Basah** |
| 5 | 79 | 0.00 | **0.10** | **0.90** | **Basah** |

**Cara menghitung manual (No. 1, Tanah = 79%):**
```
μ_Kering : 79 > 50 → μ = 0.00
μ_Lembab : 70 < 79 < 80 → μ = (80 - 79) / (80 - 70) = 1 / 10 = 0.10
μ_Basah  : 70 < 79 < 80 → μ = (79 - 70) / (80 - 70) = 9 / 10 = 0.90 ✓
Hasil    : argmax(0.00, 0.10, 0.90) = Basah
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
| 1 | 0.10 | max(0, min(0.10, 0.10)) = **0.10** | **1.00** | OFF | OFF | **ON** |
| 2 | 0.13 | max(0, min(0.10, 0.13)) = **0.10** | **1.00** | OFF | OFF | **ON** |
| 3 | 0.13 | max(0, min(0.20, 0.13)) = **0.13** | **1.00** | OFF | OFF | **ON** |
| 4 | 0.10 | max(0, min(0.10, 0.10)) = **0.10** | **1.00** | OFF | OFF | **ON** |
| 5 | 0.05 | max(0, min(0.10, 0.05)) = **0.05** | **1.00** | OFF | OFF | **ON** |
**Threshold keputusan:**
- Kipas ON jika μ_Kipas > **0.50**
- Pompa Air ON jika μ_PompaAir > **0.40**
- Pompa pH ON jika μ_PompaPH > **0.40**

---

## e. Perbandingan Perhitungan Manual dan Sistem

| No | pH | Suhu | Tanah | Perhitungan Manual | | | Perhitungan Sistem | | | Sesuai |
|----|----|----|-------|----|----|----|----|----|----|--------|
| | | | | μ pH | μ Suhu | μ Tanah | μ pH | μ Suhu | μ Tanah | |
| 1 | 5.0 | 27.4 | 79 | 1.00 | 0.90 | 0.90 | 1.00 | 0.90 | 0.90 | ✓ |
| 2 | 5.0 | 27.5 | 79 | 1.00 | 0.88 | 0.90 | 1.00 | 0.88 | 0.90 | ✓ |
| 3 | 5.0 | 27.5 | 78 | 1.00 | 0.88 | 0.80 | 1.00 | 0.88 | 0.80 | ✓ |
| 4 | 5.0 | 27.4 | 79 | 1.00 | 0.90 | 0.90 | 1.00 | 0.90 | 0.90 | ✓ |
| 5 | 5.0 | 27.2 | 79 | 1.00 | 0.95 | 0.90 | 1.00 | 0.95 | 0.90 | ✓ |

> **Keterangan kolom μ:**
> - μ pH   = nilai derajat keanggotaan himpunan **Asam** (dominan, karena pH 5.0)
> - μ Suhu = nilai derajat keanggotaan himpunan **Sedang** (dominan, karena suhu 27.x°C)
> - μ Tanah = nilai derajat keanggotaan himpunan **Basah** (dominan, karena tanah 78–79%)

---

## Kesimpulan

Dari hasil pengujian di atas yang telah dilakukan dapat dilihat bahwa
hasil perhitungan manual menggunakan Microsoft Excel dan perhitungan
pada sistem **sesuai dengan yang diharapkan**. Hal ini membuktikan
bahwa proses pengambilan keputusan menggunakan metode Fuzzy Tahani
pada sistem telah berjalan sesuai dengan aturan yang dirancang.

**Ringkasan hasil pengujian:**

- Nilai pH 5.0 → himpunan **Asam** (μ = 1.00) → Pompa pH **ON**,
  sistem memberikan koreksi larutan agar pH naik ke rentang normal (6–7.5)
- Suhu 27.2–27.5°C → himpunan **Sedang** (μ_Tinggi ≤ 0.13 < 0.50) →
  Kipas Pendingin **OFF**
- Kelembapan tanah 78–79% → himpunan **Basah** (μ_Kering = 0, μ_PompaAir ≤ 0.13 < 0.40) →
  Pompa Irigasi **OFF**

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
| 1 | 25/6/2026 13:25 | 5.0 | 27.4 | 79 | **1.00** | 0.00 | 0.00 | **Asam** | 0.00 | **0.90** | 0.10 | **Sedang** | 0.00 | 0.10 | **0.90** | **Basah** | 0.10 | 0.10 | **1.00** | OFF | OFF | **ON** |
| 2 | 25/6/2026 13:24 | 5.0 | 27.5 | 79 | **1.00** | 0.00 | 0.00 | **Asam** | 0.00 | **0.88** | 0.13 | **Sedang** | 0.00 | 0.10 | **0.90** | **Basah** | 0.13 | 0.10 | **1.00** | OFF | OFF | **ON** |
| 3 | 25/6/2026 13:23 | 5.0 | 27.5 | 78 | **1.00** | 0.00 | 0.00 | **Asam** | 0.00 | **0.88** | 0.13 | **Sedang** | 0.00 | 0.20 | **0.80** | **Basah** | 0.13 | 0.13 | **1.00** | OFF | OFF | **ON** |
| 4 | 25/6/2026 13:22 | 5.0 | 27.4 | 79 | **1.00** | 0.00 | 0.00 | **Asam** | 0.00 | **0.90** | 0.10 | **Sedang** | 0.00 | 0.10 | **0.90** | **Basah** | 0.10 | 0.10 | **1.00** | OFF | OFF | **ON** |
| 5 | 25/6/2026 13:21 | 5.0 | 27.2 | 79 | **1.00** | 0.00 | 0.00 | **Asam** | 0.00 | **0.95** | 0.05 | **Sedang** | 0.00 | 0.10 | **0.90** | **Basah** | 0.05 | 0.05 | **1.00** | OFF | OFF | **ON** |

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

> **Tips:** Format sel F2:T6 sebagai angka desimal 2 tempat dengan cara
> klik kanan → Format Cells → Number → Decimal places: 2
