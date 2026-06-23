Berikut adalah penjelasan detail mengenai konsep Logika Fuzzy yang digunakan dalam sistem *smart greenhouse* Anda. Proses ini mengubah nilai sensor mentah menjadi keputusan pintar (menyalakan/mematikan relay).

### 1. Fuzzifikasi: Trapmf dan Trimf
Fuzzifikasi adalah proses **mengubah nilai angka pasti (*crisp*) dari sensor menjadi nilai linguistik/bahasa** (seperti Rendah, Sedang, Tinggi) beserta *derajat keanggotaannya* (bernilai 0 hingga 1). Dalam sistem Anda, hal ini dilakukan menggunakan dua bentuk kurva:

*   **Trimf (Triangular / Kurva Segitiga):**
    Memiliki 3 titik pembatas (a, b, c). Kurva ini naik dari 0 ke puncak 1, lalu langsung turun lagi ke 0 membentuk segitiga.
    *   *Penggunaan:* Biasanya dipakai untuk nilai tengah (misalnya kategori **Sedang**). Karena area amannya sempit, nilainya hanya 100% (bernilai 1) pada satu titik spesifik saja.
*   **Trapmf (Trapezoidal / Kurva Trapesium):**
    Memiliki 4 titik pembatas (a, b, c, d). Kurva ini memiliki area datar di bagian puncak (bernilai 1) sebelum turun kembali ke 0.
    *   *Penggunaan:* Dipakai untuk kategori ekstrem seperti **Rendah** atau **Tinggi**. Contohnya: "Jika kelembaban di atas 80% hingga 100%, maka derajat keanggotaan 'Basah' pasti penuh (bernilai 1)".

*(Contoh: Jika Suhu 25.5°C, sistem tidak melihatnya sebagai angka mutlak, melainkan menerjemahkannya menjadi: "60% masuk kategori Sedang (Trimf) dan 40% masuk kategori Rendah (Trapmf)")*

Di file index.ino (baris 215-232), Anda telah mendefinisikan rumus matematika murni untuk mengubah nilai sensor menjadi derajat keanggotaan (Fuzzifikasi):
// trimf / trapmf: fungsi keanggotaan input & output (fuzzifikasi Tahani)
static float trimf(float x, float a, float b, float c) { ... }
static float trapmf(float x, float a, float b, float c, float d) { ... }

Lalu, fungsi ini dipanggil untuk membaca nilai sensor, contohnya untuk Suhu (baris 273):
float muR = trapmf(tempC, 0.0f, 0.0f, 24.0f, 27.0f); // Derajat Suhu Rendah
float muS = trimf(tempC, 24.0f, 27.0f, 31.0f);       // Derajat Suhu Sedang
float muT = trapmf(tempC, 27.0f, 31.0f, 45.0f, 45.0f); // Derajat Suhu Tinggi

---

### 2. Rule Base: Aturan IF-THEN
*Rule Base* adalah **otak atau kumpulan aturan manusia** yang ditanamkan ke dalam program. Logika ini menghubungkan kategori input (dari sensor) ke kategori output (untuk relay). Aturannya menggunakan format **IF (Jika) - THEN (Maka)**.

Berdasarkan kode Anda, aturan ini berbunyi:
*   **Jalur Suhu:** IF Suhu *Tinggi*, THEN Blower output *Tinggi* (ON).
*   **Jalur Tanah:** IF Kelembaban *Rendah* (Kering), THEN Pompa Air output *Tinggi* (ON).
*   **Jalur pH:** IF pH *Sangat Rendah* (Asam) ATAU pH *Sangat Tinggi* (Basa), THEN Pompa Koreksi output *Tinggi* (ON).

Aturan ini tertanam saat Anda memasangkan variabel Input dengan kategori Output di akhir fungsi pemrosesan jalur. Contoh pada kontrol kelembaban tanah (baris 287):
return fuzzyTahaniCentroid(muK, OUT_TINGGI, muL, OUT_RENDAH, muB, OUT_RENDAH);
Satu baris kode di atas mewakili 3 aturan IF-THEN sekaligus:

IF Kering (muK) THEN Pompa Air TINGGI (OUT_TINGGI)
IF Lembab (muL) THEN Pompa Air RENDAH (OUT_RENDAH)
IF Basah (muB) THEN Pompa Air RENDAH (OUT_RENDAH)

Artinya:
- Jika Tanah Kering (muK tinggi), maka Output TINGGI.
- Jika Tanah Lembab (muL tinggi), maka Output RENDAH.
- Jika Tanah Basah (muB tinggi), maka Output RENDAH.

---

### 3. Inferensi: Implikasi MIN dan Agregasi MAX
Inferensi adalah proses **mengevaluasi aturan IF-THEN** di atas berdasarkan derajat fuzzifikasi yang didapat. Sistem Anda menggunakan metode Mamdani/Tahani yang melibatkan dua tahap matematis:

*   **Implikasi MIN (Memotong Kurva):**
    Saat sistem mengetahui bahwa suhu "60% Sedang", sistem akan melihat kurva output "Sedang" untuk Relay. Fungsi **MIN** bertugas memotong (memangkas) puncak kurva output tersebut pada ketinggian 60% (0.6). *Artinya, kekuatan keputusan output dibatasi minimal sama dengan kekuatan inputnya.*
*   **Agregasi MAX (Menggabungkan Kurva):**
    Karena suhu bisa berada di antara dua batas (misal: 60% Sedang dan 40% Rendah), akan ada dua hasil pemotongan kurva output. Fungsi **MAX** bertugas menggabungkan/menumpuk kurva-kurva yang terpotong tadi menjadi **satu area bangun datar gabungan yang baru**. Jika ada area yang tumpang tindih, sistem hanya mengambil garis luar tertinggi (maksimal).

Di dalam fungsi fuzzyTahaniCentroid (baris 251), Anda bisa melihat bagaimana sistem melakukan pemotongan kurva (MIN) dan penggabungan area (MAX):
agg = fmaxfz(agg, fminfz(mu1, out_mu_table[kind1][i]));
agg = fmaxfz(agg, fminfz(mu2, out_mu_table[kind2][i]));
agg = fmaxfz(agg, fminfz(mu3, out_mu_table[kind3][i]));

fminfz memotong tinggi kurva output (out_mu_table) sebesar nilai dari sensor (mu1, mu2, dst).
fmaxfz menggabungkan semuanya menjadi bentuk bangun ruang gabungan (agg).
---

### 4. Defuzzifikasi: Centroid
Setelah Inferensi selesai, kita mendapatkan sebuah "area bangun datar abstrak" dari penggabungan kurva. Masalahnya, relay / aktuator hanya mengerti nilai angka tunggal (pasti), bukan gambar area. 

Defuzzifikasi adalah proses **mengubah kembali area abstrak tersebut menjadi satu angka pasti (*crisp*)**.

*   **Metode Centroid (Titik Berat):**
    Metode ini mencari nilai **Center of Gravity (titik tengah gravitasi)** dari area bangun datar hasil agregasi MAX tadi. 
    *   Dalam kode Anda (`num / den`), sistem membelah area tersebut menjadi 20 titik berjejer (`i * 0.05`), menghitung luas totalnya, lalu mencari titik keseimbangannya.
    *   Hasil akhirnya adalah sebuah skor berkisar antara **0.0 hingga 1.0**.
*   **Eksekusi Relay:**
    Di dalam kode Anda, ditetapkan ambang batas `RELAY_FUZZY_THRESHOLD = 0.5`. 
    Jika skor Centroid dari suatu jalur (Suhu/Tanah/pH) mencapai **0.5 atau lebih**, maka ESP32 akan menyalakan (ON) relay tersebut. Jika di bawah 0.5, relay akan dimatikan (OFF).

    Di dalam fungsi yang sama, sistem melakukan pencarian Titik Berat (Centroid) pada area gabungan yang diwakili oleh variabel agg:
    num += y * agg;
    den += agg;
    // ...
    return (den > 1e-6f) ? (num / den) : 0.0f; // Rumus Titik Berat (Centroid)
    Bagaimana dengan index.html?
Perhitungan logika fuzzy yang berat ini sepenuhnya dilakukan di dalam ESP32 (index.ino). File web index.html bertugas menerima hasil akhir dari perhitungan tersebut (berupa skor 0 hingga 1) melalui MQTT atau Supabase. Web menggunakan skor centroid tersebut (variabel fuzzy_suhu, fuzzy_soil, fuzzy_ph) untuk menggambar visualisasi di layar Anda, seperti indikator apakah status kipas/pompa saat ini sedang ON atau OFF.
'
## Penjelasan Fuzzy Tahani (Mamdani) — Sistem Greenhouse Cabai Rawit

Dokumen ini menjelaskan **perhitungan fuzzy** di firmware `index.ino`, grafik `grafik.py`, dan tampilan skor di `index.html`.  
Teori umum logika fuzzy ada di `fuzzy.text`; wiring & diagram sistem ada di `penjelasan.text`.

---

## 1. Gambaran umum

Sistem memakai **tiga mesin fuzzy terpisah** (bukan satu input gabungan):

| No | Input sensor | Skor keluaran (0–1) | Aktuator | GPIO |
|----|----------------|---------------------|----------|------|
| 1 | Suhu udara DHT22 (°C) | `fuzzy_suhu` | Servo paranet | 21 |
| 2 | Kelembaban tanah (%) | `fuzzy_soil` | Relay air | 26 |
| 3 | pH tanah | `fuzzy_ph` | Relay koreksi pH | 27 |

**Metode:** Fuzzy **Tahani** dengan inferensi **Mamdani**:
- Konsekuen = himpunan fuzzy **Rendah / Sedang / Tinggi** pada domain intensitas **[0, 1]**.
- Bukan Fuzzy **Sugeno** (konsekuen angka tunggal `z`).

**Tahapan per jalur:**
1. Fuzzifikasi input → μ (derajat keanggotaan)
2. Aturan IF-THEN
3. Implikasi **MIN**
4. Agregasi **MAX**
5. Defuzzifikasi **centroid** → skor 0–1
6. Aktuator **ON** jika skor ≥ **0,5** (`RELAY_FUZZY_THRESHOLD`)

**Grafik pendukung:**
- `grafik_fuzzy_input.png` — MF input (suhu, tanah, pH)
- `grafik_fuzzy_output.png` — MF konsekuen intensitas [0, 1]
- Generate: `python grafik.py`

---

## 2. Parameter optimal cabai rawit (acuan monitoring)

Sesuai header `index.ino` — **bukan** sama persis dengan batas fuzzy, tetapi acuan “lingkungan ideal”:

| Parameter | Nilai sesuai | Konstanta / catatan |
|-----------|----------------|---------------------|
| Suhu udara | **24–28 °C** | `TEMP_OPTIMAL_MIN`, `TEMP_OPTIMAL_MAX` |
| Kelembaban tanah | **50–70 %** | komentar header; zona **Lembab** pada fuzzy |
| pH tanah | **6–7** (netral) | zona **Netral** pada fuzzy |

**Flag MQTT `temp_optimal`:** bernilai 1 jika suhu dalam 24–28 °C (hanya informasi, bukan langsung ON/OFF paranet).

**LED GPIO2:** menyala jika kelembaban tanah **< 50 %** (`DRY_THRESHOLD_PERCENT`) — indikator kering, terpisah dari skor fuzzy.

---

## 3. Fungsi keanggotaan (fuzzifikasi)

Di `index.ino` dipakai dua bentuk (sama `grafik.py`):

### 3.1 Segitiga — `trimf(x, a, b, c)`
- Naik linear dari `a` ke `b` (μ: 0 → 1)
- Turun linear dari `b` ke `c` (μ: 1 → 0)
- Puncak μ = 1 di `x = b`

### 3.2 Trapesium — `trapmf(x, a, b, c, d)`
- Naik dari `a` ke `b`
- Plató μ = 1 antara `b` dan `c`
- Turun dari `c` ke `d`

μ selalu pada rentang **[0, 1]**.

---

## 4. Parameter fuzzifikasi INPUT (per jalur)

### 4.1 Jalur 1 — Suhu udara (°C)

| Label linguistik | Fungsi | Parameter (a, b, c, d) |
|------------------|--------|-------------------------|
| Rendah | trapmf | 0, 0, 24, 27 |
| Sedang | trimf | 24, 27, 31 |
| Tinggi | trapmf | 27, 31, 45, 45 |

**Aturan IF-THEN:**
- IF suhu **Rendah**  THEN intensitas **Rendah**
- IF suhu **Sedang**  THEN intensitas **Sedang**
- IF suhu **Tinggi**  THEN intensitas **Tinggi**

**Fungsi firmware:** `fuzzyParanetFromTemp()` → `fuzzy_suhu` → servo paranet.

**Intuisi:** Suhu **panas** (≥ ~27–31 °C) → skor cenderung tinggi → paranet ON jika ≥ 0,5.

---

### 4.2 Jalur 2 — Kelembaban tanah (%)

| Label | Fungsi | Parameter |
|-------|--------|-----------|
| Kering | trapmf | 0, 0, 40, 50 |
| Lembab | trapmf | 40, 50, 70, 80 |
| Basah | trapmf | 70, 80, 100, 100 |

**Aturan IF-THEN:**
- IF **Kering**  THEN intensitas **Tinggi** (butuh penyiraman)
- IF **Lembab** THEN intensitas **Rendah**
- IF **Basah**  THEN intensitas **Rendah**

**Fungsi firmware:** `fuzzyWaterFromSoil()` → `fuzzy_soil` → relay air.

**Intuisi:** Tanah **< ~50 %** (kering) → skor tinggi → relay air ON. Rentang **50–70 %** (lembab/optimal) → skor rendah.

**Kalibrasi ADC:** `DRY_VALUE = 3000`, `WET_VALUE = 1000` → mapping ke 0–100 %.

---

### 4.3 Jalur 3 — pH tanah

| Label | Fungsi | Parameter |
|-------|--------|-----------|
| Asam | trapmf | 3, 3, 5, 6 |
| Netral | trapmf | 5.5, 6, 7, 7.5 |
| Basa | trapmf | 7, 7.5, 9, 9 |

**Aturan IF-THEN:**
- IF **Asam**   THEN intensitas **Tinggi** (butuh koreksi larutan)
- IF **Netral** THEN intensitas **Rendah**
- IF **Basa**   THEN intensitas **Rendah**

**Fungsi firmware:** `fuzzyPhCorrectionFromPh()` → `fuzzy_ph` → relay pH.

**Validitas:** pH diproses hanya jika **3,0 ≤ pH ≤ 9,0** (`phValid`); di luar itu skor = 0 dan relay pH OFF.

**Intuisi:** pH **6–7** (netral) → skor rendah → tidak koreksi.

---

## 5. Himpunan keluaran (konsekuen Mamdani)

Domain keluaran: **y** = skor intensitas aktuator, **0 sampai 1** (`outMuByKind` di `index.ino`).

| Intensitas | Fungsi | Parameter |
|------------|--------|-----------|
| Rendah | trapmf | 0, 0, 0.15, 0.45 |
| Sedang | trimf | 0.15, 0.35, 0.55 |
| Tinggi | trapmf | 0.45, 0.65, 1, 1 |

- **Rendah (≈ 0)** → tidak perlu aktuator / intensitas minimal
- **Sedang** → kebutuhan sedang
- **Tinggi (≈ 1)** → kuat perlu aktuator

---

## 6. Inferensi: MIN dan MAX

### 6.1 Implikasi MIN

Untuk setiap aturan yang terpicu (μ input > 0):

```
μ_aturan(y) = min( μ_input , μ_keluaran(y) )
```

Artinya kurva konsekuen “dipotong” pada tinggi μ input.

### 6.2 Agregasi MAX

Gabungan semua aturan aktif:

```
μ_agregat(y) = max( μ_aturan1(y) , μ_aturan2(y) , μ_aturan3(y) )
```

---

## 7. Defuzzifikasi centroid

Skor akhir (titik berat himpunan agregat):

```
         Σ ( y × μ_agregat(y) )
skor = ─────────────────────────
              Σ μ_agregat(y)
```

- **y** disampling 0, 0.05, 0.10, …, 1.00 (21 titik, `STEPS = 20` di ESP32).
- Hasil: **skor ∈ [0, 1]** → dikirim sebagai `fuzzy_suhu`, `fuzzy_soil`, `fuzzy_ph`.

**Implementasi:** fungsi `mamdaniTahaniCentroid(mu1, kind1, mu2, kind2, mu3, kind3)` di `index.ino`.

Pseudocode:
```
num = 0, den = 0
untuk i = 0 .. 20:
  y = i / 20
  agg = 0
  agg = max( agg, min(mu1, outMu(y, kind1)) )
  agg = max( agg, min(mu2, outMu(y, kind2)) )
  agg = max( agg, min(mu3, outMu(y, kind3)) )
  num += y * agg
  den += agg
skor = num / den   (jika den > 0)
```

---

## 8. Keputusan aktuator (ambang 0,5)

```cpp
const float RELAY_FUZZY_THRESHOLD = 0.5f;
```

| Kondisi | Aktuator |
|---------|----------|
| skor ≥ 0,5 | ON (servo sudut ON / relay LOW jika aktif-LOW) |
| skor < 0,5 | OFF |

Dashboard `index.html`: `FUZZY_THRESHOLD = 0.5` — teks hijau jika skor ≥ 0,5.

---

## 9. Contoh perhitungan konseptual

### 9.1 Suhu 30 °C

**Fuzzifikasi (perkiraan):**
- μ Rendah ≈ 0
- μ Sedang ≈ 0,67 (dominan)
- μ Tinggi ≈ 0,25

**Aturan:** Sedang→Sedang, Tinggi→Tinggi (keduanya aktif).

**Centroid:** skor sekitar **0,35–0,55** (tergantung bentuk agregat) — mendekati sedang–agak tinggi. Jika ≥ 0,5 → paranet ON.

---

### 9.2 Kelembaban tanah 35 %

**Fuzzifikasi:**
- μ Kering tinggi (~0,5 di transisi)
- μ Lembab mulai naik
- μ Basah ≈ 0

**Aturan dominan:** Kering → intensitas **Tinggi**.

**Centroid:** skor cenderung **> 0,5** → **relay air ON**.

---

### 9.3 pH 6,5

**Fuzzifikasi:**
- μ Asam rendah
- μ **Netral** tinggi (plató 6–7)
- μ Basa rendah

**Aturan:** Netral → intensitas **Rendah**.

**Centroid:** skor mendekati **0** → **relay pH OFF** (sesuai target pH 6–7).

---

### 9.4 pH 5,2 (asam)

**Fuzzifikasi:** μ Asam tinggi.

**Aturan:** Asam → intensitas **Tinggi**.

**Centroid:** skor tinggi → **relay pH ON** jika ≥ 0,5.

---

## 10. Alur di `loop()` (satu siklus)

1. Baca DHT22 → suhu `t`, kelembaban udara `h`
2. Baca soil ADC → `moisturePercent` (0–100 %)
3. Baca pH (DMS + ADC) → `ph`, validasi 3–9
4. `scoreParanet = fuzzyParanetFromTemp(t, dhtOk)`
5. `scoreSoil = fuzzyWaterFromSoil(moisturePercent)`
6. `scorePh = fuzzyPhCorrectionFromPh(ph, phValid)`
7. Bandingkan dengan 0,5 → servo / relay
8. Publish MQTT (~10 s) & insert Supabase (~30 s) berisi skor + status

**Urutan penting untuk laporan:** fuzzy & aktuator **sebelum** kirim ke cloud (beda beberapa contoh TA yang fuzzy setelah database).

---

## 11. Perbedaan Tahani/Mamdani vs Sugeno

| Aspek | Tahani/Mamdani (`index.ino` sekarang) | Sugeno (versi lama) |
|-------|--------------------------------------|---------------------|
| Konsekuen | Kurva fuzzy Rendah/Sedang/Tinggi | Angka tetap z (mis. 0, 0.25, 1) |
| Penggabungan | MIN → MAX → **centroid** | `(μ₁z₁ + μ₂z₂ + μ₃z₃) / (μ₁+μ₂+μ₃)` |
| Grafik laporan | Input + output MF | Hanya input |
| Nama di MQTT/DB | `fuzzy_suhu`, `fuzzy_soil`, `fuzzy_ph` | sama (kolom tetap skor 0–1) |

---

## 12. Tabel ringkas untuk bab TA

| Tahap | Apa yang terjadi | Bukti di proyek |
|-------|------------------|-----------------|
| Fuzzifikasi | Hitung μ tiap label input | `trimf`/`trapmf`, `grafik_fuzzy_input.png` |
| Aturan | IF input linguistik THEN intensitas | komentar di `fuzzyXxxFrom...` |
| MIN–MAX | Potong & gabung kurva keluaran | `mamdaniTahaniCentroid` |
| Centroid | Skor 0–1 | `fuzzy_suhu`, `fuzzy_soil`, `fuzzy_ph` |
| Kontrol diskret | ON/OFF aktuator | `RELAY_FUZZY_THRESHOLD = 0.5` |

---

## 13. File terkait

| File | Peran |
|------|------|
| `index.ino` | Implementasi fuzzy + kontrol aktuator |
| `grafik.py` | Plot MF input & output |
| `grafik_fuzzy_input.png` | Gambar fuzzifikasi |
| `grafik_fuzzy_output.png` | Gambar konsekuen |
| `index.html` | Tampilan skor real-time & riwayat |
| `flowchart_sistem.png` | Alur fuzzy sebelum cloud |
| `fuzzy.text` | Teori logika fuzzy umum |
| `penjelasan.text` | Diagram blok, wiring, flowchart |

---

## 14. Kalimat siap pakai untuk laporan

"Sistem kontrol greenhouse cabai rawit menggunakan **Fuzzy Tahani (Mamdani) tiga jalur independen** untuk suhu udara, kelembaban tanah, dan pH tanah. Setiap jalur melalui fuzzifikasi dengan fungsi keanggotaan segitiga dan trapesium, inferensi aturan IF-THEN, implikasi MIN, agregasi MAX, dan defuzzifikasi **metode centroid** menghasilkan skor intensitas 0–1. Aktuator (servo paranet, relay air, relay pH) aktif bila skor ≥ 0,5, selaras parameter optimal budidaya cabai rawit (suhu 24–28 °C, kelembaban tanah 50–70 %, pH 6–7)."
