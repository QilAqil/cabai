# Representasi Grafik Fungsi Keanggotaan Suhu Udara

---

## Parameter Fungsi Keanggotaan

Variabel suhu udara dibagi menjadi **3 himpunan fuzzy** dengan
parameter sebagai berikut:

| Himpunan | Fungsi | Parameter |
|----------|--------|-----------|
| Rendah   | Trapesium kiri (trapmf) | (0, 0, 24, 27) |
| Sedang   | Segitiga (trimf)        | (24, 27, 31)   |
| Tinggi   | Trapesium kanan (trapmf)| (27, 31, 45, 45) |

---

## Grafik Fungsi Keanggotaan

```
Derajat
Keanggotaan
μ(x)
  │
1 ├────────┐          ╱╲              ┌──────────
  │        │         ╱  ╲            │
  │        │        ╱    ╲          ╱
  │        │       ╱      ╲        ╱
  │        │      ╱        ╲      ╱
  │        │     ╱          ╲    ╱
  │        │    ╱            ╲  ╱
0 ├────────┴───────────────────────────────────→
  0       24   27            31               45
           Suhu Udara (°C)

  ━━━━━━  Rendah     ─ ─ ─  Sedang     ·····  Tinggi
```

---

## Penjelasan Setiap Himpunan

### 1. Himpunan Rendah — trapmf(0, 0, 24, 27)

```cpp
static float fuzzyTempRendah(float x) {
  if (x <= 24.0f)                    return 1.0f;
  else if (x > 24.0f && x < 27.0f)  return (27.0f - x) / 3.0f;
  else                               return 0.0f;
}
```

Bentuk **trapesium dengan shoulder di kiri**:

| Rentang Suhu | Nilai μ | Penjelasan |
|---|---|---|
| 0°C – 24°C | 1.0 | Sepenuhnya Rendah |
| 24°C – 27°C | (27 – x) / 3 | Menurun linier dari 1 ke 0 |
| ≥ 27°C | 0.0 | Bukan Rendah |

Contoh perhitungan untuk suhu 25°C:
```
μ_Rendah = (27 - 25) / (27 - 24) = 2 / 3 = 0.667
```

---

### 2. Himpunan Sedang — trimf(24, 27, 31)

```cpp
static float fuzzyTempSedang(float x) {
  if (x <= 24.0f || x >= 31.0f)     return 0.0f;
  else if (x > 24.0f && x <= 27.0f) return (x - 24.0f) / 3.0f;
  else                               return (31.0f - x) / 4.0f;
}
```

Bentuk **segitiga** dengan puncak di 27°C:

| Rentang Suhu | Nilai μ | Penjelasan |
|---|---|---|
| ≤ 24°C atau ≥ 31°C | 0.0 | Bukan Sedang |
| 24°C – 27°C | (x – 24) / 3 | Naik dari 0 ke 1 |
| 27°C – 31°C | (31 – x) / 4 | Turun dari 1 ke 0 |

Contoh perhitungan untuk suhu 25°C:
```
μ_Sedang = (25 - 24) / (27 - 24) = 1 / 3 = 0.333
```

Contoh perhitungan untuk suhu 29°C:
```
μ_Sedang = (31 - 29) / (31 - 27) = 2 / 4 = 0.500
```

---

### 3. Himpunan Tinggi — trapmf(27, 31, 45, 45)

```cpp
static float fuzzyTempTinggi(float x) {
  if (x <= 27.0f)                    return 0.0f;
  else if (x > 27.0f && x < 31.0f)  return (x - 27.0f) / 4.0f;
  else                               return 1.0f;
}
```

Bentuk **trapesium dengan shoulder di kanan**:

| Rentang Suhu | Nilai μ | Penjelasan |
|---|---|---|
| ≤ 27°C | 0.0 | Bukan Tinggi |
| 27°C – 31°C | (x – 27) / 4 | Naik dari 0 ke 1 |
| ≥ 31°C | 1.0 | Sepenuhnya Tinggi |

Contoh perhitungan untuk suhu 29°C:
```
μ_Tinggi = (29 - 27) / (31 - 27) = 2 / 4 = 0.500
```

---

## Contoh Perhitungan Lengkap

Untuk suhu **25.4°C** (dari data log sistem):

```
μ_Rendah = (27.0 - 25.4) / (27.0 - 24.0) = 1.6 / 3.0 = 0.533
μ_Sedang = (25.4 - 24.0) / (27.0 - 24.0) = 1.4 / 3.0 = 0.467
μ_Tinggi = 0  (25.4 ≤ 27, belum masuk zona Tinggi)
```

Status → argmax → **Rendah** (0.533 terbesar)

Karena μ_Tinggi = 0, maka **Kipas Pendingin = OFF**

---

## Zona Transisi (Tumpang Tindih)

Pada suhu **24°C – 27°C**, dua himpunan aktif bersamaan:

```
Suhu 25°C:
  μ_Rendah = 0.667  ← dominan
  μ_Sedang = 0.333
  μ_Tinggi = 0.000

Suhu 26°C:
  μ_Rendah = 0.333
  μ_Sedang = 0.667  ← dominan
  μ_Tinggi = 0.000

Suhu 27°C (titik persimpangan):
  μ_Rendah = 0.000
  μ_Sedang = 1.000  ← puncak Sedang
  μ_Tinggi = 0.000
```

Inilah keunggulan fuzzy — suhu 25°C tidak langsung dikategorikan
"Rendah" atau "Sedang" secara kaku, melainkan **53.3% Rendah dan
33.3% Sedang** secara bersamaan.

---

## Hubungan dengan Aktuator

Himpunan suhu digunakan pada 2 rule inferensi:

```
Rule 1: IF suhu TINGGI → KIPAS ON
        Threshold: μ_Tinggi > 0.5
        Kipas menyala jika suhu sudah cukup di atas 29°C

Rule 3: IF tanah LEMBAB AND suhu TINGGI → POMPA AIR ON
        Operator AND = min(μ_Lembab, μ_Tinggi)
        Pompa air menyala jika tanah lembab DAN suhu tinggi
```

---

## Kode Pembuatan Grafik (Python)

```python
x_suhu = np.linspace(0, 45, 2000)

suhu_rendah = trapmf(x_suhu, 0,  0,  24, 27)
suhu_sedang = trimf( x_suhu, 24, 27, 31)
suhu_tinggi = trapmf(x_suhu, 27, 31, 45, 45)
```

**`np.linspace(0, 45, 2000)`**
Membuat 2000 titik data dari 0°C sampai 45°C untuk menghasilkan
kurva yang halus saat digambar.

**`trapmf(x, a, b, c, d)`**
Fungsi trapesium dengan 4 parameter:
- `a, b` = sisi kiri (a = mulai naik, b = puncak kiri)
- `c, d` = sisi kanan (c = puncak kanan, d = mulai turun)
- Jika `a == b` → shoulder kiri (nilai 1 dari awal)
- Jika `c == d` → shoulder kanan (nilai 1 hingga akhir)

**`trimf(x, a, b, c)`**
Fungsi segitiga dengan 3 parameter:
- `a` = titik mulai naik (μ = 0)
- `b` = titik puncak (μ = 1)
- `c` = titik selesai turun (μ = 0)

---

*File ini menjelaskan representasi grafik fungsi keanggotaan
suhu udara pada sistem Fuzzy Tahani monitoring greenhouse cabai rawit.*
