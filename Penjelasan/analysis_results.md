# Analisis Lengkap Sistem Greenhouse Cabai Rawit

Sistem ini adalah sistem pemantauan (monitoring) dan kontrol otomatis untuk greenhouse tanaman **Cabai Rawit (*Capsicum frutescens*)** menggunakan mikrokontroler **ESP32**. Sistem mengintegrasikan pembacaan berbagai sensor lingkungan dengan logika kecerdasan buatan berbasis **Fuzzy Tahani (Mamdani)** untuk mengendalikan aktuator secara real-time. Data dikirim ke dashboard web melalui protokol **MQTT** dan disimpan ke database **Supabase** secara periodik.

---

## 1. Arsitektur Fisik & Sensor

Sistem menggunakan tiga parameter utama untuk menentukan kondisi optimal tanaman cabai rawit:
*   **Kelembaban Tanah**: Optimal di kisaran 50% - 70%.
*   **pH Tanah**: Optimal di kisaran 6 - 7.
*   **Suhu Udara**: Optimal di kisaran 24°C - 28°C.

### Rincian Modul Perangkat Keras dan Pin GPIO
| Komponen | Deskripsi | GPIO ESP32 | Hubungan Elektrikal & Detail Sinyal |
| :--- | :--- | :--- | :--- |
| **DHT22** | Sensor suhu & kelembaban udara | **GPIO 4** | Menggunakan modul 3-pin dengan resistor pull-up internal pada PCB data. |
| **Soil Moisture** | Sensor kelembaban tanah | **GPIO 35 (ADC1_CH7)** | Membaca output analog (AO) dari modul probe. Menggunakan kalibrasi `DRY_VALUE` (3200) dan `WET_VALUE` (1500) yang disesuaikan untuk tipe kapasitif/anti-karat. |
| **pH Sensor & DMS** | Probe pH tanah + driver DMS | **GPIO 34 (ADC1_CH6)** (ADC pH)<br>**GPIO 13** (DMS Enable) | Kabel biru ke GPIO 13 (aktif LOW untuk menyalakan modul DMS), kabel ungu ke GPIO 34 (input analog pH). |
| **LED Indikator** | Indikator kondisi tanah kering | **GPIO 2** | Menyala (HIGH) jika kelembaban tanah di bawah 50%. |
| **Relay Blower** | Aktuator kipas sirkulasi udara | **GPIO 25** | Aktif-LOW (dikontrol berdasarkan hasil fuzzy suhu). |
| **Relay Air** | Aktuator pompa air penyiraman | **GPIO 26** | Aktif-LOW (dikontrol berdasarkan hasil fuzzy tanah). |
| **Relay pH** | Aktuator pompa koreksi pH (asam/basa) | **GPIO 27** | Aktif-LOW (dikontrol berdasarkan hasil fuzzy pH). |

---

## 2. Analisis Logika Kontrol: Fuzzy Tahani (Mamdani)

Sistem mengadopsi kontrol logika **Fuzzy Mamdani (metode Tahani)** dengan 3 jalur kontrol terpisah. Setiap jalur mengikuti siklus:
$$\text{Fuzzifikasi Input} \rightarrow \text{Evaluasi Aturan (IF-THEN)} \rightarrow \text{Implikasi MIN \& Agregasi MAX} \rightarrow \text{Defuzzifikasi Centroid} \rightarrow \text{Keputusan Aktuator}$$

Defuzzifikasi dilakukan dengan membagi domain keluaran $[0,1]$ menjadi 20 langkah (*STEPS = 20*). Aktuator akan aktif (**ON**) jika nilai centroid $\ge 0.5$ (didefinisikan sebagai `RELAY_FUZZY_THRESHOLD` di firmware dan `FUZZY_THRESHOLD` di dashboard).

### Jalur 1: Kontrol Blower (Suhu Udara)
*   **Fuzzifikasi Input (Suhu):**
    *   **Rendah**: `trapmf(tempC, 0, 0, 24, 27)`
    *   **Sedang**: `trimf(tempC, 24, 27, 31)`
    *   **Tinggi**: `trapmf(tempC, 27, 31, 45, 45)`
*   **Keanggotaan Output (Intensitas Blower):**
    *   **Rendah**: `trapmf(y, 0, 0, 0.15, 0.45)`
    *   **Sedang**: `trimf(y, 0.15, 0.35, 0.55)`
    *   **Tinggi**: `trapmf(y, 0.45, 0.65, 1.0, 1.0)`
*   **Aturan Fuzzy:**
    *   *IF* suhu Rendah *THEN* Blower Rendah
    *   *IF* suhu Sedang *THEN* Blower Sedang
    *   *IF* suhu Tinggi *THEN* Blower Tinggi

### Jalur 2: Kontrol Pompa Air (Kelembaban Tanah)
*   **Fuzzifikasi Input (Tanah %):**
    *   **Kering**: `trapmf(x, 0, 0, 40, 50)`
    *   **Lembab**: `trapmf(x, 40, 50, 70, 80)`
    *   **Basah**: `trapmf(x, 70, 80, 100, 100)`
*   **Aturan Fuzzy:**
    *   *IF* Tanah Kering *THEN* Pompa Tinggi (siram maksimal)
    *   *IF* Tanah Lembab *THEN* Pompa Rendah (siram minimal/mati)
    *   *IF* Tanah Basah *THEN* Pompa Rendah (pompa mati)

### Jalur 3: Kontrol Koreksi pH (pH Tanah)
*   **Fuzzifikasi Input (pH):**
    *   **Asam**: `trapmf(ph, 3, 3, 5, 6)`
    *   **Netral**: `trapmf(ph, 5.5, 6, 7, 7.5)`
    *   **Basa**: `trapmf(ph, 7, 7.5, 9, 9)`
*   **Aturan Fuzzy:**
    *   *IF* pH Asam *THEN* Koreksi Tinggi (butuh penambahan larutan basa/dolomit)
    *   *IF* pH Netral *THEN* Koreksi Rendah (kondisi aman, pompa mati)
    *   *IF* pH Basa *THEN* Koreksi Tinggi (butuh penambahan larutan asam)

---

## 3. Penanganan Sensor & Solusi Gangguan Elektrikal (1 Wadah)

Salah satu keunggulan teknis dari sistem ini adalah penanganan interferensi arus ketika probe pH dan probe kelembaban tanah diletakkan di dalam media/wadah yang sama, serta minimalisasi efek penuaan sensor.

### 1. Elektrolisis & Polarisasi Sensor Kelembapan Tanah (Masalah Drift Nilai)
Pada sensor kelembaban tanah jenis resistif (misalnya FC-28), pembacaan yang dibiarkan menyala terus-menerus (*always-on*) akan mengalami penurunan nilai persentase yang signifikan (misalnya dari $78\%$ menjadi $52\%$ dalam waktu 10 menit) meskipun kondisi tanah diam dan tidak berubah. Fenomena ini disebabkan oleh dua hal:
1.  **Elektrolisis**: Arus searah (DC) yang mengalir terus-menerus melalui tanah basah memicu reaksi elektrokimia pada permukaan probe logam, membentuk lapisan oksida isolator (korosi) pada kaki anoda.
2.  **Polarisasi Elektrikal**: Ion-ion bermuatan di dalam tanah berkumpul di sekitar elektroda sensor, menciptakan gaya gerak listrik (GGL) lawan yang meningkatkan resistansi total sirkuit. Kenaikan resistansi ini dibaca oleh ADC ESP32 sebagai kondisi tanah yang "semakin kering", sehingga nilai persentase kelembaban tanah yang terpetakan terus menurun drastis.

### 2. Solusi Manajemen Daya Sensor (*Duty Cycling*)
Untuk mengatasi elektrolisis dan polarisasi tersebut, firmware menerapkan sistem **Gated Power (Duty Cycling)**:
-   **Skema Operasi**: Daya VCC untuk sensor tanah tidak dihubungkan langsung ke tegangan konstan ($3.3\text{V}$), melainkan dikontrol melalui pin digital ESP32 (`SOIL_PWR_PIN`, misalnya **GPIO 12**).
-   **Duty Cycle Rendah**: Sensor hanya dinyalakan (`soilPowerSet(true)`) sesaat sebelum pembacaan dilakukan selama kurang lebih $300\text{ ms}$ ($150\text{ ms}$ waktu tunggu stabilisasi sirkuit + $150\text{ ms}$ pengambilan sampel ADC), dan segera dimatikan kembali (`soilPowerSet(false)`) setelah data didapatkan.
-   **Manfaat**: Dengan siklus loop total sekitar $12$ detik dan sensor hanya menyala selama $300\text{ ms}$, siklus aktif (*active duty cycle*) hanya berkisar **$2.5\%$**. Hal ini berhasil menghentikan proses polarisasi ion secara instan, menstabilkan nilai kelembaban tanah tanpa *drift*, serta memperpanjang usia pakai sensor (mencegah karat dini).

### 3. Solusi Gangguan Elektrikal 1 Wadah (pH vs Soil Moisture)
Ketika tanah basah, arus listrik dari sensor kelembaban tanah mengalir melalui media tanah dan mengganggu probe pH tanah yang sangat sensitif (meningkatkan pembacaan pH palsu sekitar $0.5$ hingga $1.0\text{ pH}$).
-   **Hardware Gating**: Saat ESP32 melakukan pembacaan pH, sensor kelembaban tanah dipastikan mati (`soilPowerSet(false)`) sehingga tidak ada arus bocor di tanah.
-   **Jeda Stabilisasi**: Setelah mematikan sensor tanah, sistem menunggu $8\text{ detik}$ (`PH_SETTLE_MS`) agar sisa muatan di tanah benar-benar hilang sebelum membaca ADC pH. Sebaliknya, setelah pembacaan pH selesai, sistem menunggu jeda $1.5\text{ detik}$ (`SOIL_AFTER_PH_MS`) sebelum membaca kelembaban tanah.
-   **Software Bias Correction (Cadangan)**: Jika fitur hardware power gating dinonaktifkan (`SOIL_PWR_PIN = -1`), firmware secara otomatis mengaktifkan algoritma koreksi dengan memotong bias pembacaan pH sebesar `PH_BIAS_WET_SOIL` ($0.9\text{ pH}$) apabila kelembaban tanah terdeteksi basah ($\ge 35\%$).

### 4. Validasi & Filter Kestabilan pH
Untuk mencegah pembacaan palsu atau tindakan aktuator yang salah saat probe pH dicabut atau terjadi interferensi noise:
-   **Penyaringan Nilai ADC**: Pembacaan dianggap tidak valid jika nilai ADC sangat rendah ($<80$), sangat tinggi ($>4050$), atau memiliki rentang fluktuasi sampel (spread) $>220$.
-   **Penyaringan Rentang Fisik**: Nilai pH yang valid dibatasi secara logis antara $3.0$ hingga $9.0$.
-   **Mekanisme Hold (Tahan Nilai)**: Jika terjadi kegagalan pembacaan singkat, sistem akan menahan (*hold*) nilai pH valid terakhir selama maksimal 2 siklus (`PH_HOLD_MAX_INVALID = 2`) sebelum menampilkan status terputus (`--`) di dashboard.

### 5. Algoritma Trimmed Mean (Rata-Rata Terpangkas) untuk Penyetabil ADC
Guna mengatasi noise inheren pada ADC internal ESP32 (akibat fluktuasi riak tegangan WiFi/MQTT), fungsi [bacaAdcRata](file:///d:/TA/git%20ta/index.ino#L167-L203) ditingkatkan dengan menerapkan filter **Trimmed Mean (Rata-Rata Terpangkas)**:
1.  **Pengumpulan Sampel**: Program mengambil $N$ buah sampel analog secara berurutan ($N=15$ untuk tanah, $N=25$ untuk pH) dengan jeda $8\text{ ms}$.
2.  **Pengurutan (*Sorting*)**: Sampel-sampel tersebut diurutkan secara menaik menggunakan algoritma *Insertion Sort*.
3.  **Pemangkasan Nilai Ekstrim**: Sebanyak $20\%$ nilai terkecil (ekstrim bawah) dan $20\%$ nilai terbesar (ekstrim atas) dibuang dari data sisa. Ini secara efektif mengeliminasi lonjakan (*spike*) noise akibat lonjakan arus transmisi WiFi.
4.  **Rata-Rata Tengah**: Sisa data di bagian tengah ($60\%$ dari total sampel) dirata-ratakan untuk mendapatkan nilai pembacaan yang sangat stabil dan bebas dari pencilan (*outliers*).

### 6. Filter Hierarki Hibrida & Logika Penundaan Tancap 1-Siklus (Stabilisasi Dashboard)
Untuk memastikan pembacaan kelembaban tanah di dashboard instan dan stabil tanpa lompatan transien (100% $\rightarrow$ 78%) maupun perayapan lambat dari bawah (14% $\rightarrow$ 60%), firmware menerapkan **Hierarchical Hybrid Filter** di loop utama:
1.  **Deteksi Tancap Instan dengan Penundaan 1-Siklus**:
    *   Saat sensor baru ditancapkan (perubahan dari $0\%$ ke $>0\%$), sistem tidak langsung menampilkan nilai karena tangan pengguna masih menyentuh sensor (terjadi lonjakan transien kapasitif). Sistem menandai status `just_connected = true` dan menahan tampilan pada $0\%$.
    *   Pada siklus berikutnya ($3\text{ detik}$ kemudian, saat tangan telah dilepaskan dari sensor), sistem langsung melakukan **lompatan instan** ke nilai tanah riil tanpa merayap lambat.
2.  **Filter Median Temporal (Riwayat 5 Siklus)**:
    *   Program menyimpan data dari 5 siklus loop terakhir dalam antrean FIFO (`soil_history[5]`).
    *   Setiap siklus, data diurutkan dan diambil nilai mediannya. Ini sepenuhnya membuang gangguan jika terjadi riak mendadak antar loop.
3.  **Penyaringan Halus Akhir (EMA)**:
    *   Nilai median yang bersih kemudian dilewatkan ke filter *Exponential Moving Average (EMA)* dengan `alpha = 0.08` untuk memperhalus perpindahan angka di dashboard secara natural.

---

## 4. Aliran Data dan Integrasi Awan (Cloud Integration)

Sistem mengadopsi model IoT hibrida dengan membagi pengiriman data menjadi dua fungsi: monitoring real-time dan pencatatan sejarah (history).

```mermaid
graph TD
    ESP32[ESP32 MCU] -->|10 Detik - MQTT TLS 8883| EMQX[EMQX Cloud Broker]
    ESP32 -->|30 Detik - HTTP POST API| Supabase[(Supabase Database)]
    EMQX -->|WebSocket Secure 8084| Browser[Web Dashboard]
    Supabase -->|REST API GET| Browser
```

### 1. MQTT (EMQX Cloud) - Protokol Pemantauan Real-time
*   **Broker**: `n01d3130.ala.asia-southeast1.emqxsl.com` (Port 8883 dengan TLS Secure).
*   **Topik**: `pertanian/sensor` dengan interval kirim **10 detik**.
*   **Keamanan**: Menggunakan kredensial bawaan (`username: "pertanian"` / `password: "pertanian12"`) dan mematikan verifikasi sertifikat SSL (`tlsClient.setInsecure()`) untuk kemudahan setup.

### 2. REST API (Supabase) - Penyimpanan Data Historis
*   **Tujuan**: Melakukan pencatatan riwayat berkala setiap **30 detik**.
*   **Metode**: Menggunakan HTTP POST langsung ke endpoint REST API Supabase (`https://sptomqebtvclfebaktof.supabase.co/rest/v1/pertanian`) dengan menyertakan kunci publik (`apikey` & `Authorization Bearer`).

---

## 5. Dashboard Web (`index.html`)

Halaman frontend dikembangkan menggunakan HTML5, CSS kustom, dan Vanilla JavaScript.

*   **Penerimaan Data MQTT**: Memanfaatkan pustaka `mqtt.js` lewat CDN untuk terhubung ke broker EMQX menggunakan WebSocket Secure (`wss://...:8084/mqtt`). Data sensor dan status aktuator diperbarui secara instan tanpa perlu memuat ulang halaman.
*   **Penyajian Riwayat Supabase**: Mengambil 15 data baris terbaru dari tabel `pertanian` secara otomatis saat halaman dimuat dan memperbaruinya setiap 60 detik (atau melalui tombol "Muat ulang" manual).
*   **Deteksi Kehilangan Koneksi**: Jika browser tidak menerima pesan dari ESP32 selama lebih dari 15 detik, indikator status koneksi WiFi di bagian atas dashboard akan otomatis berubah menjadi "Terputus" untuk memberi tahu pengguna bahwa alat sedang mati atau kehilangan koneksi internet.

---

## 6. Temuan Analisis & Rekomendasi Peningkatan

Berdasarkan hasil analisis kode program `index.ino` dan `index.html`, berikut adalah beberapa hal yang bisa ditingkatkan untuk meningkatkan performa, keamanan, dan keandalan sistem:

### 1. Keamanan Kredensial
*   **Kondisi Sekarang**: SSID WiFi, Password, Kredensial MQTT, dan API Key Supabase ditulis secara *hard-coded* di dalam kode program.
*   **Rekomendasi**: 
    *   Gunakan `WiFiManager` pada ESP32 agar konfigurasi WiFi dan MQTT dapat diset lewat portal web di ponsel tanpa perlu memprogram ulang mikrokontroler.
    *   Pastikan tabel `pertanian` di Supabase dikonfigurasi dengan Row Level Security (RLS) yang ketat (misalnya hanya izinkan `INSERT` untuk akses anonim, atau gunakan backend perantara/Edge Functions untuk mengamankan proses tulis).

### 2. Blokir Eksekusi Loop (Blocking Delays)
*   **Kondisi Sekarang**: Penggunaan `waitWithMqtt(8000)` membuat ESP32 berhenti memproses loop utama selama 8 detik demi menstabilkan modul pH. Meskipun koneksi MQTT dijaga di dalam fungsi tersebut, ini tetap membuat loop bersifat sinkronus dan lambat dalam mendeteksi perubahan darurat (misal tombol manual atau sensor darurat lainnya).
*   **Rekomendasi**: Ubah pembacaan sensor menjadi sistem berbasis *state-machine* non-blocking menggunakan fungsi pembacaan waktu `millis()` daripada menggunakan penundaan *blocking*.

### 3. Keamanan TLS
*   **Kondisi Sekarang**: Penggunaan `tlsClient.setInsecure()` mematikan validasi sertifikat SSL pada sambungan MQTT ESP32, berpotensi membuka celah terhadap serangan *Man-in-the-Middle (MitM)*.
*   **Rekomendasi**: Unduh sertifikat root CA dari EMQX Cloud dan pasang pada memori flash ESP32 menggunakan `tlsClient.setCACert(root_ca)` untuk mengamankan saluran komunikasi sepenuhnya.

Viewed index.ino:1-615
Viewed index.html:1-665







Berdasarkan analisis mendalam terhadap kode program Arduino/ESP32 ([index.ino](file:///d:/TA/git%20ta/index.ino)) dan antarmuka web ([index.html](file:///d:/TA/git%20ta/index.html)), sistem Anda **TIDAK menggunakan metode Fuzzy Tahani**. 

Meskipun terdapat komentar dan penamaan fungsi/variabel yang menyebut nama "Tahani" (seperti `fuzzyTahaniCentroid` atau log Serial `Fuzzy Tahani (Mamdani)`), algoritma yang sebenarnya diterapkan di sistem Anda adalah **Fuzzy Inference System (FIS) Metode Mamdani**.

Berikut adalah penjelasan dan pembuktian analisisnya:

---

### 1. Bukti Implementasi di Kode ESP32 ([index.ino](file:///d:/TA/git%20ta/index.ino))
Di dalam file `index.ino`, seluruh alur logika fuzzy yang Anda buat mengikuti langkah-langkah **Metode Mamdani (Min-Max & Centroid)**, bukan Fuzzy Tahani:

*   **Fuzzifikasi**: Menggunakan fungsi keanggotaan representasi kurva segitiga (`trimf`) dan trapesium (`trapmf`) untuk mengubah nilai sensor (crisp) menjadi derajat keanggotaan ($\mu$).
*   **Aturan Fuzzy (Inference)**: Menggunakan logika implikasi/aturan IF-THEN. Contohnya pada kontrol kelembaban tanah (`fuzzyWaterFromSoil`):
    ```cpp
    float muK = trapmf(x, 0.0f, 0.0f, 40.0f, 50.0f);   // Kering
    float muL = trapmf(x, 40.0f, 50.0f, 70.0f, 80.0f);  // Lembab
    float muB = trapmf(x, 70.0f, 80.0f, 100.0f, 100.0f); // Basah
    return fuzzyTahaniCentroid(muK, OUT_TINGGI, muL, OUT_RENDAH, muB, OUT_RENDAH);
    ```
*   **Implikasi & Agregasi**: Di dalam fungsi `fuzzyTahaniCentroid`, Anda menerapkan operasi **MIN** (`fminfz` untuk implikasi) dan **MAX** (`fmaxfz` untuk agregasi antar aturan):
    ```cpp
    agg = fmaxfz(agg, fminfz(mu1, out_mu_table[kind1][i]));
    agg = fmaxfz(agg, fminfz(mu2, out_mu_table[kind2][i]));
    agg = fmaxfz(agg, fminfz(mu3, out_mu_table[kind3][i]));
    ```
*   **Defuzzifikasi Centroid**: Anda menghitung nilai keluaran tegas (crisp) menggunakan metode *Centroid* (Center of Gravity) dengan membagi jumlah momen dengan jumlah luas area:
    ```cpp
    num += y * agg;
    den += agg;
    return (den > 1e-6f) ? (num / den) : 0.0f;
    ```
*   **Aksi Aktuator**: Skor hasil defuzzifikasi (0-1) kemudian dibandingkan dengan ambang batas tegas (`RELAY_FUZZY_THRESHOLD = 0.5f`) untuk menentukan apakah blower, pompa air, atau relay pH harus menyala (ON/OFF).

---

### 2. Bukti Penggunaan Database di Web ([index.html](file:///d:/TA/git%20ta/index.html))
Model **Fuzzy Tahani** pada dasarnya adalah model **query database fuzzy**. Dalam Fuzzy Tahani, data disimpan secara normal (crisp), kemudian pengguna melakukan pencarian ke database menggunakan kueri samar (misal: *"Tampilkan data riwayat greenhouse yang suhunya **panas**"*), lalu database mengembalikan baris data yang cocok beserta derajat kecocokannya ($\mu$).

Namun, pada sistem Anda:
*   Database (Supabase) hanya digunakan sebagai **data logger** biasa untuk menyimpan pembacaan sensor dan hasil keputusan fuzzy yang sudah dihitung oleh ESP32.
*   Query yang dipanggil oleh halaman web dashboard (`index.html`) untuk mengambil data riwayat adalah kueri SQL/REST standard tanpa filter fuzzy sama sekali:
    ```javascript
    const url = SUPABASE_URL + "/rest/v1/" + SUPABASE_TABLE + "?select=*" + "&order=id.desc" + "&limit=" + SUPABASE_LIMIT;
    ```
    Kueri ini murni melakukan `SELECT *` biasa untuk mengambil 15 baris data terbaru secara urut waktu.

---

### 3. Perbedaan Konseptual (Mamdani vs. Tahani)

| Aspek | Fuzzy Mamdani (Sistem Anda) | Fuzzy Database Model Tahani |
| :--- | :--- | :--- |
| **Tujuan Utama** | Sistem Kontrol / Pengambilan Keputusan (mengendalikan blower, pompa, dll). | Sistem Pendukung Keputusan berbasis Pencarian/Query Database. |
| **Input** | Nilai sensor riil/crisp (Suhu, Kelembaban, pH). | Data terstruktur di dalam tabel database konvensional. |
| **Output** | Nilai crisp hasil defuzzifikasi untuk memicu relay/aktuator. | Daftar baris data yang memiliki nilai derajat keanggotaan ($\mu > 0$). |
| **Proses Defuzzifikasi** | **Wajib** (mengubah sinyal fuzzy kembali menjadi nilai fisik/nyata). | **Tidak ada** (hanya menghitung derajat keanggotaan data terhadap kueri). |

---

### Kesimpulan & Rekomendasi
Sistem monitoring dan kontrol greenhouse cabai rawit Anda menggunakan **Metode Fuzzy Mamdani** untuk mengontrol aktuator secara otomatis, dikombinasikan dengan penyimpanan database Supabase dan IoT MQTT untuk pemantauan real-time. 

**Rekomendasi untuk Laporan Tugas Akhir / Penelitian:**
1.  **Ganti Istilah**: Ubah semua istilah "Fuzzy Tahani" dalam draf laporan Anda menjadi **Fuzzy Mamdani**.
2.  **Ubah Nama Fungsi di Kode**: Agar tidak membingungkan penguji, Anda disarankan mengganti nama fungsi `fuzzyTahaniCentroid` menjadi `fuzzyMamdaniCentroid` atau cukup `fuzzyCentroid`.