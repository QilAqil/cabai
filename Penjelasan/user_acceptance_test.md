# User Acceptance Test (UAT)
## Sistem Pertanian Cerdas Cabai Rawit Berbasis IoT
## Metode Fuzzy Tahani

---

## A. Pendahuluan

User Acceptance Test (UAT) adalah pengujian akhir yang dilakukan oleh
pengguna untuk memastikan sistem yang dibangun telah memenuhi kebutuhan
dan dapat berjalan sesuai dengan yang diharapkan. Pengujian ini dilakukan
oleh 3 responden yang menggunakan sistem secara langsung dan memberikan
penilaian terhadap fungsionalitas serta kemudahan penggunaan sistem.

**Sistem yang diuji:** Sistem Monitoring dan Pengendalian Greenhouse
Cabai Rawit berbasis IoT menggunakan metode Fuzzy Tahani

**Komponen yang diuji:**
1. Pembacaan dan tampilan data sensor real-time
2. Pengendalian otomatis aktuator berdasarkan metode Fuzzy Tahani
3. Dashboard web monitoring
4. Kontrol manual aktuator dari dashboard
5. Penyimpanan dan tampilan riwayat data

---

## B. Data Responden

| No | Nama | Jabatan / Peran | Tanggal Uji |
|----|------|-----------------|-------------|
| 1  | .................. | Petani Cabai Rawit | .............. |
| 2  | .................. | Dosen Pembimbing   | .............. |
| 3  | .................. | Teknisi Pertanian  | .............. |

---

## C. Skala Penilaian

| Nilai | Keterangan |
|-------|------------|
| 5 | Sangat Baik |
| 4 | Baik |
| 3 | Cukup |
| 2 | Kurang |
| 1 | Sangat Kurang |

---

## D. Lembar Pengujian

### Responden 1

**Nama   :** .................................
**Jabatan:** .................................
**Tanggal:** .................................

| No | Aspek Pengujian | Skenario Uji | Hasil yang Diharapkan | Nilai (1-5) | Keterangan |
|----|----------------|--------------|-----------------------|-------------|------------|
| 1 | Tampilan Dashboard | Buka dashboard di browser | Halaman terbuka, kartu sensor tampil, status MQTT terhubung | | |
| 2 | Data Sensor Real-Time | Amati nilai suhu, kelembaban udara, kelembaban tanah, dan pH | Nilai berubah setiap 15 detik sesuai kondisi lingkungan | | |
| 3 | Status Koneksi WiFi | Perhatikan header dashboard | Tampil nama WiFi, IP address, dan kekuatan sinyal | | |
| 4 | Badge Status Fuzzy | Amati badge di bawah nilai sensor | Tampil label Rendah/Sedang/Tinggi, Kering/Lembab/Basah, Asam/Normal/Basa | | |
| 5 | Indikator Relay Otomatis | Biarkan sistem berjalan otomatis | Relay Kipas, Pompa Air, Pompa pH menyala/mati sesuai kondisi sensor | | |
| 6 | Pengendalian Kipas | Naikkan suhu di atas 31°C | Kipas Pendingin menyala otomatis (relay ON) | | |
| 7 | Pengendalian Pompa Air | Kurangi kelembaban tanah di bawah 40% | Pompa Irigasi menyala otomatis (relay ON) | | |
| 8 | Pengendalian Pompa pH | Pastikan pH tanah di bawah 6 | Pompa Koreksi pH menyala otomatis (relay ON) | | |
| 9 | Kontrol Manual — Switch | Geser switch ke mode Manual | Badge berubah menjadi MANUAL, tombol aktuator aktif | | |
| 10 | Kontrol Manual — Kipas | Mode manual, klik tombol Kipas, klik Kirim | Kipas menyala sesuai perintah dari dashboard | | |
| 11 | Kontrol Manual — Pompa Air | Mode manual, klik tombol Pompa Air, klik Kirim | Pompa air menyala sesuai perintah dari dashboard | | |
| 12 | Kontrol Manual — Pompa pH | Mode manual, klik tombol Pompa pH, klik Kirim | Pompa pH menyala sesuai perintah dari dashboard | | |
| 13 | Kembali Mode Otomatis | Geser switch kembali ke Otomatis | Sistem kembali ke pengendalian fuzzy otomatis | | |
| 14 | Tabel Riwayat Data | Amati tabel RIWAYAT DATA (SUPABASE) | Menampilkan data historis dengan waktu, nilai sensor, dan status relay | | |
| 15 | Auto-Refresh Tabel | Tunggu 30 detik | Tabel riwayat otomatis diperbarui tanpa reload halaman | | |
| 16 | Tombol Muat Ulang | Klik tombol ↺ Muat ulang | Tabel riwayat langsung diperbarui | | |
| 17 | Deteksi Koneksi Terputus | Matikan ESP32 selama 60 detik | Dashboard menampilkan status "Terputus" di header | | |
| 18 | Kemudahan Penggunaan | Nilai kemudahan penggunaan dashboard secara keseluruhan | Dashboard mudah dipahami dan dioperasikan | | |

**Total Nilai Responden 1:**

| Kategori | Aspek No | Jumlah Nilai | Rata-rata |
|----------|----------|--------------|-----------|
| Tampilan & Koneksi | 1, 2, 3, 4 | | |
| Pengendalian Otomatis | 5, 6, 7, 8 | | |
| Kontrol Manual | 9, 10, 11, 12, 13 | | |
| Riwayat Data | 14, 15, 16 | | |
| Deteksi & Kemudahan | 17, 18 | | |
| **Total Keseluruhan** | **1–18** | | |

**Persentase Kepuasan = (Total Nilai / Nilai Maksimum) × 100%**
```
Nilai Maksimum = 18 aspek × 5 = 90
Persentase     = ...... / 90 × 100% = ......%
```

**Saran dan Catatan Responden 1:**
```
.................................................................
.................................................................
```

---

### Responden 2

**Nama   :** .................................
**Jabatan:** .................................
**Tanggal:** .................................

| No | Aspek Pengujian | Skenario Uji | Hasil yang Diharapkan | Nilai (1-5) | Keterangan |
|----|----------------|--------------|-----------------------|-------------|------------|
| 1 | Tampilan Dashboard | Buka dashboard di browser | Halaman terbuka, kartu sensor tampil, status MQTT terhubung | | |
| 2 | Data Sensor Real-Time | Amati nilai suhu, kelembaban udara, kelembaban tanah, dan pH | Nilai berubah setiap 15 detik sesuai kondisi lingkungan | | |
| 3 | Status Koneksi WiFi | Perhatikan header dashboard | Tampil nama WiFi, IP address, dan kekuatan sinyal | | |
| 4 | Badge Status Fuzzy | Amati badge di bawah nilai sensor | Tampil label Rendah/Sedang/Tinggi, Kering/Lembab/Basah, Asam/Normal/Basa | | |
| 5 | Indikator Relay Otomatis | Biarkan sistem berjalan otomatis | Relay Kipas, Pompa Air, Pompa pH menyala/mati sesuai kondisi sensor | | |
| 6 | Pengendalian Kipas | Naikkan suhu di atas 31°C | Kipas Pendingin menyala otomatis (relay ON) | | |
| 7 | Pengendalian Pompa Air | Kurangi kelembaban tanah di bawah 40% | Pompa Irigasi menyala otomatis (relay ON) | | |
| 8 | Pengendalian Pompa pH | Pastikan pH tanah di bawah 6 | Pompa Koreksi pH menyala otomatis (relay ON) | | |
| 9 | Kontrol Manual — Switch | Geser switch ke mode Manual | Badge berubah menjadi MANUAL, tombol aktuator aktif | | |
| 10 | Kontrol Manual — Kipas | Mode manual, klik tombol Kipas, klik Kirim | Kipas menyala sesuai perintah dari dashboard | | |
| 11 | Kontrol Manual — Pompa Air | Mode manual, klik tombol Pompa Air, klik Kirim | Pompa air menyala sesuai perintah dari dashboard | | |
| 12 | Kontrol Manual — Pompa pH | Mode manual, klik tombol Pompa pH, klik Kirim | Pompa pH menyala sesuai perintah dari dashboard | | |
| 13 | Kembali Mode Otomatis | Geser switch kembali ke Otomatis | Sistem kembali ke pengendalian fuzzy otomatis | | |
| 14 | Tabel Riwayat Data | Amati tabel RIWAYAT DATA (SUPABASE) | Menampilkan data historis dengan waktu, nilai sensor, dan status relay | | |
| 15 | Auto-Refresh Tabel | Tunggu 30 detik | Tabel riwayat otomatis diperbarui tanpa reload halaman | | |
| 16 | Tombol Muat Ulang | Klik tombol ↺ Muat ulang | Tabel riwayat langsung diperbarui | | |
| 17 | Deteksi Koneksi Terputus | Matikan ESP32 selama 60 detik | Dashboard menampilkan status "Terputus" di header | | |
| 18 | Kemudahan Penggunaan | Nilai kemudahan penggunaan dashboard secara keseluruhan | Dashboard mudah dipahami dan dioperasikan | | |

**Total Nilai Responden 2:**

| Kategori | Aspek No | Jumlah Nilai | Rata-rata |
|----------|----------|--------------|-----------|
| Tampilan & Koneksi | 1, 2, 3, 4 | | |
| Pengendalian Otomatis | 5, 6, 7, 8 | | |
| Kontrol Manual | 9, 10, 11, 12, 13 | | |
| Riwayat Data | 14, 15, 16 | | |
| Deteksi & Kemudahan | 17, 18 | | |
| **Total Keseluruhan** | **1–18** | | |

**Persentase Kepuasan = (Total Nilai / Nilai Maksimum) × 100%**
```
Nilai Maksimum = 18 aspek × 5 = 90
Persentase     = ...... / 90 × 100% = ......%
```

**Saran dan Catatan Responden 2:**
```
.................................................................
.................................................................
```

---

### Responden 3

**Nama   :** .................................
**Jabatan:** .................................
**Tanggal:** .................................

| No | Aspek Pengujian | Skenario Uji | Hasil yang Diharapkan | Nilai (1-5) | Keterangan |
|----|----------------|--------------|-----------------------|-------------|------------|
| 1 | Tampilan Dashboard | Buka dashboard di browser | Halaman terbuka, kartu sensor tampil, status MQTT terhubung | | |
| 2 | Data Sensor Real-Time | Amati nilai suhu, kelembaban udara, kelembaban tanah, dan pH | Nilai berubah setiap 15 detik sesuai kondisi lingkungan | | |
| 3 | Status Koneksi WiFi | Perhatikan header dashboard | Tampil nama WiFi, IP address, dan kekuatan sinyal | | |
| 4 | Badge Status Fuzzy | Amati badge di bawah nilai sensor | Tampil label Rendah/Sedang/Tinggi, Kering/Lembab/Basah, Asam/Normal/Basa | | |
| 5 | Indikator Relay Otomatis | Biarkan sistem berjalan otomatis | Relay Kipas, Pompa Air, Pompa pH menyala/mati sesuai kondisi sensor | | |
| 6 | Pengendalian Kipas | Naikkan suhu di atas 31°C | Kipas Pendingin menyala otomatis (relay ON) | | |
| 7 | Pengendalian Pompa Air | Kurangi kelembaban tanah di bawah 40% | Pompa Irigasi menyala otomatis (relay ON) | | |
| 8 | Pengendalian Pompa pH | Pastikan pH tanah di bawah 6 | Pompa Koreksi pH menyala otomatis (relay ON) | | |
| 9 | Kontrol Manual — Switch | Geser switch ke mode Manual | Badge berubah menjadi MANUAL, tombol aktuator aktif | | |
| 10 | Kontrol Manual — Kipas | Mode manual, klik tombol Kipas, klik Kirim | Kipas menyala sesuai perintah dari dashboard | | |
| 11 | Kontrol Manual — Pompa Air | Mode manual, klik tombol Pompa Air, klik Kirim | Pompa air menyala sesuai perintah dari dashboard | | |
| 12 | Kontrol Manual — Pompa pH | Mode manual, klik tombol Pompa pH, klik Kirim | Pompa pH menyala sesuai perintah dari dashboard | | |
| 13 | Kembali Mode Otomatis | Geser switch kembali ke Otomatis | Sistem kembali ke pengendalian fuzzy otomatis | | |
| 14 | Tabel Riwayat Data | Amati tabel RIWAYAT DATA (SUPABASE) | Menampilkan data historis dengan waktu, nilai sensor, dan status relay | | |
| 15 | Auto-Refresh Tabel | Tunggu 30 detik | Tabel riwayat otomatis diperbarui tanpa reload halaman | | |
| 16 | Tombol Muat Ulang | Klik tombol ↺ Muat ulang | Tabel riwayat langsung diperbarui | | |
| 17 | Deteksi Koneksi Terputus | Matikan ESP32 selama 60 detik | Dashboard menampilkan status "Terputus" di header | | |
| 18 | Kemudahan Penggunaan | Nilai kemudahan penggunaan dashboard secara keseluruhan | Dashboard mudah dipahami dan dioperasikan | | |

**Total Nilai Responden 3:**

| Kategori | Aspek No | Jumlah Nilai | Rata-rata |
|----------|----------|--------------|-----------|
| Tampilan & Koneksi | 1, 2, 3, 4 | | |
| Pengendalian Otomatis | 5, 6, 7, 8 | | |
| Kontrol Manual | 9, 10, 11, 12, 13 | | |
| Riwayat Data | 14, 15, 16 | | |
| Deteksi & Kemudahan | 17, 18 | | |
| **Total Keseluruhan** | **1–18** | | |

**Persentase Kepuasan = (Total Nilai / Nilai Maksimum) × 100%**
```
Nilai Maksimum = 18 aspek × 5 = 90
Persentase     = ...... / 90 × 100% = ......%
```

**Saran dan Catatan Responden 3:**
```
.................................................................
.................................................................
```

---

## E. Rekapitulasi Hasil UAT

### Tabel Rekapitulasi Per Aspek

| No | Aspek Pengujian | R1 | R2 | R3 | Total | Rata-rata |
|----|----------------|----|----|----|-------|-----------|
| 1 | Tampilan Dashboard | | | | | |
| 2 | Data Sensor Real-Time | | | | | |
| 3 | Status Koneksi WiFi | | | | | |
| 4 | Badge Status Fuzzy | | | | | |
| 5 | Indikator Relay Otomatis | | | | | |
| 6 | Pengendalian Kipas | | | | | |
| 7 | Pengendalian Pompa Air | | | | | |
| 8 | Pengendalian Pompa pH | | | | | |
| 9 | Kontrol Manual — Switch | | | | | |
| 10 | Kontrol Manual — Kipas | | | | | |
| 11 | Kontrol Manual — Pompa Air | | | | | |
| 12 | Kontrol Manual — Pompa pH | | | | | |
| 13 | Kembali Mode Otomatis | | | | | |
| 14 | Tabel Riwayat Data | | | | | |
| 15 | Auto-Refresh Tabel | | | | | |
| 16 | Tombol Muat Ulang | | | | | |
| 17 | Deteksi Koneksi Terputus | | | | | |
| 18 | Kemudahan Penggunaan | | | | | |
| | **Total** | | | | | |

### Tabel Rekapitulasi Per Kategori

| Kategori | R1 | R2 | R3 | Rata-rata | % |
|----------|----|----|----|-----------|---|
| Tampilan & Koneksi (No 1–4) | | | | | |
| Pengendalian Otomatis (No 5–8) | | | | | |
| Kontrol Manual (No 9–13) | | | | | |
| Riwayat Data (No 14–16) | | | | | |
| Deteksi & Kemudahan (No 17–18) | | | | | |
| **Total Keseluruhan** | | | | | |

### Persentase Kepuasan Keseluruhan

```
Total Nilai Maksimum = 3 responden × 18 aspek × 5 = 270

Total Nilai Aktual   = R1 + R2 + R3 = ......

Persentase UAT = (Total Nilai Aktual / 270) × 100%
               = ...... / 270 × 100%
               = ......%
```

### Skala Penerimaan

| Persentase | Keterangan |
|------------|------------|
| 81% – 100% | Sangat Layak / Sangat Diterima |
| 61% – 80%  | Layak / Diterima |
| 41% – 60%  | Cukup Layak |
| 21% – 40%  | Kurang Layak |
| 0% – 20%   | Tidak Layak |

---

## F. Kesimpulan UAT

Berdasarkan hasil pengujian User Acceptance Test yang telah dilakukan
oleh 3 responden dengan total 18 aspek pengujian, diperoleh persentase
penerimaan sistem sebesar **......%**. Hal ini menunjukkan bahwa sistem
pertanian cerdas cabai rawit berbasis IoT dengan metode Fuzzy Tahani
yang telah dibangun **................** dan dapat diterima oleh pengguna.

---

*Dokumen UAT ini merupakan bagian dari pengujian sistem pada
Tugas Akhir Program Studi Teknik Informatika.*
