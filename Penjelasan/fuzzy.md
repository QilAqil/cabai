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
