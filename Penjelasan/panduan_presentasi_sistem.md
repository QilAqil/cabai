# Panduan Presentasi: Sistem IoT Pertanian Cerdas Cabai Rawit

> **Judul Sistem:** Sistem Monitoring dan Kontrol Otomatis Pertanian Cabai Rawit Berbasis IoT dan Logika Fuzzy Tahani  
> **Platform:** ESP32 FreeRTOS + MQTT + Supabase + Dashboard Web  
> **Estimasi Durasi:** 25–35 menit

---

## 📌 Roadmap Presentasi: Pendekatan "Kerangka Pikir"
*Sangat disarankan untuk menampilkan gambar "Kerangka Pikir" (Flowchart Problem -> Result) di awal presentasi sebagai peta jalan (roadmap) bagi penguji. Panduan ini disusun persis mengikuti alur kerangka pikir tersebut agar cara Anda menjelaskan menjadi sangat runut, logis, dan terstruktur.*

---

## TAHAP 1: Problem, Approach & Opportunity (Latar Belakang)
**Durasi: 2–3 menit | Slide: Latar Belakang & Tujuan**

**💡 Cara Menjelaskan:** 
Awali presentasi dengan memaparkan **PROBLEM** (masalah) di lapangan, lalu tawarkan solusi berupa **APPROACH** (pendekatan) teknisnya, dan sebutkan **OPPORTUNITY** (peluang/manfaat) yang didapat.

*   **PROBLEM:** Sampaikan bahwa pertumbuhan cabai rawit sangat bergantung pada 3 variabel: pH tanah, suhu, dan kelembaban. Saat ini, petani kesulitan memantau secara kontinu dan ketiganya sering tidak optimal, sehingga menghambat panen. Keputusan penyiraman pun hanya mengandalkan "feeling".
*   **APPROACH:** Kenalkan solusi Anda: *"Oleh karena itu, kami membangun sistem pertanian cerdas berbasis IoT menggunakan metode Fuzzy Tahani untuk memantau dan mengendalikan kondisi lingkungan tanaman cabai secara otomatis."*
*   **OPPORTUNITY:** Jelaskan nilainya: *"Peluangnya adalah kita bisa mengintegrasikan monitoring real-time dan kontrol presisi, menggantikan intuisi petani dengan keputusan berbasis data yang akurat."*

---

## TAHAP 2: System Development - Analisis & Desain (Arsitektur Sistem)
**Durasi: 3–4 menit | Slide: Diagram Blok & Arsitektur**

**💡 Cara Menjelaskan:** 
Masuk ke blok **System Development**. Jelaskan bagaimana Anda merancang *Data, Analisis, Desain, dan Coding*. Gunakan diagram arsitektur sistem (Blok Sistem) untuk menjelaskan alur dari Hardware hingga ke Cloud.

*   **Sensor Layer (Data):** Sebutkan hardware yang dipakai (DHT22, Soil Moisture, Sensor pH) untuk mengambil data lingkungan mentah (suhu, kelembaban, pH).
*   **Processing Layer (Coding & Analisis):** Jelaskan inovasi **Dual-Core FreeRTOS** pada C++ ESP32.
    *   *Core 1 (Task Sensor):* Bertugas membaca sensor, memfilter noise, dan memproses perhitungan logika Fuzzy Tahani.
    *   *Core 0 (Task Komunikasi):* Bertugas menjaga koneksi MQTT dan mengirim data ke Supabase tanpa mengganggu (non-blocking) waktu baca sensor.
*   **Cloud Layer (Desain):** Data sistem ini tidak disimpan di memori kecil, melainkan dikirim via protokol MQTT (EMQX untuk real-time) dan API Supabase (untuk menyimpan riwayat/historis).

---

## TAHAP 3: System Development - Logika Fuzzy Tahani (Core Otak Sistem)
**Durasi: 5–7 menit | Slide: Fuzzifikasi, Rule Base, Defuzzifikasi**

**💡 Cara Menjelaskan:** 
Ini adalah inti/otak dari kecerdasan sistem Anda. Ajak penguji membedah kotak putus-putus **"FUZZY TAHANI"** pada kerangka pikir. Jelaskan langkahnya berurutan:

### 1. Input Data & Fuzzifikasi
Jelaskan bahwa data mentah dari sensor diubah menjadi nilai derajat keanggotaan (bernilai 0 sampai 1). Tampilkan grafik fungsinya (Trapesium/Segitiga).
*   **Suhu:** Direpresentasikan dalam himpunan Rendah, Sedang, Tinggi.
*   **Tanah:** Direpresentasikan dalam himpunan Kering, Lembab, Basah.
*   **pH:** Direpresentasikan dalam himpunan Asam, Normal, Basa.
*   *Analogi presentasi:* *"Bapak/Ibu, tanah dengan kadar air 49% itu tidak mutlak dinilai 'kering' atau mutlak 'lembab', tapi memiliki derajat keanggotaan di kedua kondisi tersebut (Fuzzy). Ini sangat mirip dengan cara insting petani menilai lahan."*

### 2. Rule Base (Aturan IF-THEN) & Inferensi
Sebutkan 4 aturan (rule) pakar yang ditanamkan untuk mengambil keputusan:
*   `R1: IF Suhu = Tinggi → Kipas Pendingin ON`
*   `R2: IF Tanah = Kering → Pompa Air Irigasi ON`
*   `R3: IF Tanah = Lembab AND Suhu = Tinggi → Pompa Air Irigasi ON` 
    *   *(Jelaskan di sini bahwa Anda menggunakan operator implikasi MIN untuk kondisi AND, dan diagregasi dengan MAX).*
*   `R4: IF pH = Asam → Pompa Koreksi pH ON`

### 3. Defuzzifikasi & Output Otomatis
Jelaskan bagaimana perhitungan Fuzzy tadi dikonversi kembali menjadi perintah tegangan nyata (aktuator).
*   *"Dari proses inferensi, kita dapatkan nilai tegas. Menggunakan metode Centroid/Threshold, jika nilai tersebut melampaui batas yang kita tentukan, maka sistem menghasilkan output otomatis untuk menyalakan Relay Kipas, Relay Pompa Air, atau Relay Pompa pH."*

---

## TAHAP 4: System Implementation (Penerapan Hardware & Web)
**Durasi: 4–5 menit | Slide: Implementasi Sensor, Aktuator & Dashboard**

**💡 Cara Menjelaskan:** 
Beralih ke blok **System Implementation**. Ceritakan bagaimana teori desain dan Fuzzy tadi diterapkan di dunia nyata (diterapkan pada greenhouse / lahan).

### 1. Implementasi Sensor (Tantangan Teknis)
*   Sampaikan tantangan terbesarnya: **Sensor pH analog**. Jelaskan bahwa sensor pH rentan error karena *interferensi galvanik* saat dicelupkan di tanah yang sama dengan sensor Soil.
*   *Cara menjual inovasi Anda:* *"Untuk mengatasi hal ini, sistem kami tidak asal baca nilai. Kami menerapkan algoritma Filter 6 Tahap (pengambilan 150 sampel, pembuangan noise ekstrem, dan moving average) agar nilai pH yang masuk ke perhitungan Fuzzy benar-benar valid."*

### 2. Implementasi Aktuator (Sistem Timer Aman)
*   Jelaskan manajemen pompanya: *"Aktuator kami tidak bekerja terus-menerus. Jika Fuzzy memerintahkan ON, pompa hanya menyala 5 detik, kemudian mengunci (jeda) selama 30 menit. Ini meniru petani asli agar tanaman tidak mati karena kelebihan air atau overdosis larutan pH."*

### 3. Tampilan Dashboard Web
*   Tunjukkan implementasi UI-nya. *"Semua data dari ESP32 dikirim ke dashboard berbasis HTML/Javascript (tanpa framework berat). Petani bisa melihat nilai sensor real-time via MQTT, memantau grafiknya, dan bisa mengambil alih kendali sistem menjadi mode MANUAL dari jarak jauh."*

---

## TAHAP 5: System Measurement (Pengukuran, Pengujian & Demo)
**Durasi: 5–7 menit | Slide: Hasil Pengujian & Demo Live**

**💡 Cara Menjelaskan:** 
Masuk ke blok **System Measurement**. Di sinilah Anda mempresentasikan hasil evaluasi bahwa sistem berjalan akurat sesuai yang tertulis di kerangka pikir.

### 1. Sampaikan Hasil Evaluasi Kinerja
*   **Kestabilan Sensor:** Tunjukkan data bahwa kalibrasi pH dan Suhu sudah akurat (toleransi error sangat kecil) dibandingkan alat ukur konvensional.
*   **Pengiriman MQTT:** Keberhasilan pengiriman data nyaris 100% tanpa delay berarti berkat arsitektur FreeRTOS.
*   **Kesesuaian Keputusan Fuzzy:** Sampaikan dengan bangga bahwa output aktuator dari mikrokontroler (ESP32) sudah diuji silang dan **100% sesuai** dengan perhitungan rumus matematis Fuzzy Tahani secara manual.

### 2. Lakukan Demo Live (Jika Memungkinkan)
Jika diminta demo, ikuti alur ini:
1.  Buka **Dashboard Web**, perlihatkan nilai berubah secara real-time.
2.  Perlihatkan **Serial Monitor ESP32**, tunjukkan log berjalannya filter sensor dan status Fuzzy (contoh: Suhu Tinggi -> µ_Tinggi = 1.0).
3.  Berikan **Trigger Fisik**: Pegang DHT22 agar suhu naik, atau angkat/keringkan sensor tanah. Tunjukkan bagaimana indikator perhitungan Fuzzy di web bereaksi dan merubah status kipas/pompa secara Otomatis.
4.  Demokan **Manual Override**: Ubah mode ke MANUAL dari web, klik tombol pompa, dan tunjukkan respon seketika (delay < 50ms) pada alat.
5.  Perlihatkan tabel Riwayat (Supabase) terisi otomatis tiap 1 menit.

---

## TAHAP 6: Result (Kesimpulan)
**Durasi: 1–2 menit | Slide: Kesimpulan**

**💡 Cara Menjelaskan:** 
Tutup presentasi dengan membacakan kotak paling bawah dari kerangka pikir, yaitu **RESULT**. 

*   *"Sebagai penutup, mengacu pada kerangka pikir awal kami, Sistem pertanian cerdas berbasis IoT menggunakan metode Fuzzy Tahani ini telah **berhasil** diwujudkan untuk memantau dan menangani kondisi lingkungan tanaman cabai rawit."*
*   *"Sistem secara terbukti mampu memantau pH tanah, kelembaban, dan suhu udara secara real-time, serta berhasil mengambil keputusan cerdas untuk mengendalikan pompa air, pompa koreksi pH, dan kipas secara otomatis demi pertumbuhan cabai rawit yang optimal."*

---

## Antisipasi Pertanyaan Kritis Penguji

*Siapkan mental dan hafalkan poin-poin ini jika penguji menggali lebih dalam:*

**Q: Mengapa harus pakai metode Fuzzy Tahani? Bukankah pakai IF-ELSE biasa (misal if tanah < 50% maka pompa ON) sudah cukup dan lebih ringan?**
> "Karena kondisi parameter alam tidak bersifat mutlak/biner, Pak/Bu. Tanah di angka 49% tidak serta merta berubah sifat menjadi sepenuhnya beda dibanding 51%. Logika Fuzzy menggunakan derajat keanggotaan, sehingga penilaian sistem itu gradual dan proporsional. Ini membuat transisi aktuator lebih halus dan sangat mewakili (merepresentasikan) insting alami manusia (petani) dalam menilai lahan."

**Q: Kenapa repot-repot menggunakan sistem operasi FreeRTOS (dual-core) pada ESP32?**
> "Karena kebutuhan sensor pH kami, Pak/Bu. Pembacaan pH analog agar stabil membutuhkan delay blocking sekitar 5 detik. Jika diprogram dengan cara biasa (Single loop), koneksi internet (MQTT) akan putus karena 'tertahan' delay 5 detik tersebut. Dengan FreeRTOS, Core 1 fokus mengurus delay sensor pH, sedangkan Core 0 bebas hambatan menjaga koneksi internet dan pengiriman data ke dashboard."

**Q: Di lahan nyata, bacaan sensor pH sering kacau kalau didekatkan dengan sensor tanah. Bagaimana Anda mengatasinya?**
> "Betul, itu disebut *interferensi galvanik*. Kami menyadari itu, makanya keunggulan sistem kami ada di algoritma filter software-nya. Kami memberi jeda waktu ukur 1.5 detik antar sensor, menerapkan *Noise Gate* untuk membuang data yang lompat tiba-tiba (outlier), dan meratakannya pakai *Moving Average* agar data pH yang masuk ke Fuzzy benar-benar murni."

**Q: Apakah data tanaman aman tersimpan untuk analisis jangka panjang?**
> "Sangat aman. Selain mengirim payload MQTT untuk dilihat seketika di dashboard, secara bersamaan sistem (di background) melakukan HTTP POST ke database Cloud Supabase setiap 1 menit untuk diarsipkan selamanya."

**Q: Kalau internet / router WiFi di kebun mati, apakah tanaman bisa mati karena penyiraman otomatisnya berhenti bekerja?**
> "Sama sekali tidak, Pak/Bu. Perhitungan logika Fuzzy berjalan murni secara *lokal* di dalam otak mikrokontroler ESP32. Jika WiFi putus, ESP32 tetap rutin membaca sensor dan tetap menyalakan pompa/kipas secara otomatis berdasar rule Fuzzy. Koneksi Cloud hanya kami gunakan sebagai layer Monitoring dan Remote (Manual) jarak jauh."
