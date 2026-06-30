# User Acceptance Test (UAT)
## Sistem Pertanian Cerdas Cabai Rawit Berbasis IoT

---

## UAT-01 — Monitoring Sensor Real-Time

| | |
|---|---|
| **Nama Sistem** | Sistem Monitoring dan Pengendalian Greenhouse Cabai Rawit Menggunakan Fuzzy Tahani Berbasis IoT |
| **Nomor Pengujian** | UAT-01 |
| **Topik Pengujian** | Monitoring data sensor secara real-time pada dashboard |
| **Tanggal Pengujian** | .............................................................. |
| **Penguji** | ➢ Bapak Ipat &nbsp;&nbsp; ➢ Bapak Rumadi &nbsp;&nbsp; ➢ Bapak Rohman |

| Penguji | Fungsi Pokok | Sesuai Ya | Sesuai Tidak |
|---------|-------------|-----------|--------------|
| | Dashboard terbuka di browser dan menampilkan kartu sensor | 1 | |
| Bapak Ipat | Nilai suhu udara tampil dan berubah setiap 15 detik | 1 | |
| | Nilai kelembaban tanah tampil dan berubah setiap 15 detik | 1 | |
| | Nilai pH tanah tampil dan berubah setiap 15 detik | 1 | |
| | Dashboard terbuka di browser dan menampilkan kartu sensor | 1 | |
| Bapak Rumadi | Nilai suhu udara tampil dan berubah setiap 15 detik | 1 | |
| | Nilai kelembaban tanah tampil dan berubah setiap 15 detik | 1 | |
| | Nilai pH tanah tampil dan berubah setiap 15 detik | 1 | |
| | Dashboard terbuka di browser dan menampilkan kartu sensor | 1 | |
| Bapak Rohman | Nilai suhu udara tampil dan berubah setiap 15 detik | 1 | |
| | Nilai kelembaban tanah tampil dan berubah setiap 15 detik | 1 | |
| | Nilai pH tanah tampil dan berubah setiap 15 detik | 1 | |
| | **Jumlah** | **12** | **0** |

---

## UAT-02 — Status Koneksi dan Informasi WiFi

| | |
|---|---|
| **Nama Sistem** | Sistem Monitoring dan Pengendalian Greenhouse Cabai Rawit Menggunakan Fuzzy Tahani Berbasis IoT |
| **Nomor Pengujian** | UAT-02 |
| **Topik Pengujian** | Tampilan status koneksi MQTT dan informasi WiFi ESP32 |
| **Tanggal Pengujian** | .............................................................. |
| **Penguji** | ➢ Bapak Ipat &nbsp;&nbsp; ➢ Bapak Rumadi &nbsp;&nbsp; ➢ Bapak Rohman |

| Penguji | Fungsi Pokok | Sesuai Ya | Sesuai Tidak |
|---------|-------------|-----------|--------------|
| | Status MQTT menampilkan "Terhubung" saat ESP32 aktif | 1 | |
| Bapak Ipat | Nama WiFi (SSID) dan IP address ESP32 tampil di header | 1 | |
| | Status berubah "Terputus" saat ESP32 dimatikan | 1 | |
| | Status MQTT menampilkan "Terhubung" saat ESP32 aktif | 1 | |
| Bapak Rumadi | Nama WiFi (SSID) dan IP address ESP32 tampil di header | 1 | |
| | Status berubah "Terputus" saat ESP32 dimatikan | 1 | |
| | Status MQTT menampilkan "Terhubung" saat ESP32 aktif | 1 | |
| Bapak Rohman | Nama WiFi (SSID) dan IP address ESP32 tampil di header | 1 | |
| | Status berubah "Terputus" saat ESP32 dimatikan | 1 | |
| | **Jumlah** | **9** | **0** |

---

## UAT-03 — Pengendalian Otomatis Berbasis Fuzzy Tahani

| | |
|---|---|
| **Nama Sistem** | Sistem Monitoring dan Pengendalian Greenhouse Cabai Rawit Menggunakan Fuzzy Tahani Berbasis IoT |
| **Nomor Pengujian** | UAT-03 |
| **Topik Pengujian** | Pengendalian aktuator secara otomatis menggunakan metode Fuzzy Tahani |
| **Tanggal Pengujian** | .............................................................. |
| **Penguji** | ➢ Bapak Ipat &nbsp;&nbsp; ➢ Bapak Rumadi &nbsp;&nbsp; ➢ Bapak Rohman |

| Penguji | Fungsi Pokok | Sesuai Ya | Sesuai Tidak |
|---------|-------------|-----------|--------------|
| | Kipas Pendingin menyala otomatis saat suhu berstatus Tinggi (> 31°C) | 1 | |
| Bapak Ipat | Pompa Irigasi menyala otomatis saat kelembaban tanah berstatus Kering (< 40%) | 1 | |
| | Pompa Koreksi pH menyala otomatis saat pH tanah berstatus Asam (< 6) | 1 | |
| | Kipas Pendingin menyala otomatis saat suhu berstatus Tinggi (> 31°C) | 1 | |
| Bapak Rumadi | Pompa Irigasi menyala otomatis saat kelembaban tanah berstatus Kering (< 40%) | 1 | |
| | Pompa Koreksi pH menyala otomatis saat pH tanah berstatus Asam (< 6) | 1 | |
| | Kipas Pendingin menyala otomatis saat suhu berstatus Tinggi (> 31°C) | 1 | |
| Bapak Rohman | Pompa Irigasi menyala otomatis saat kelembaban tanah berstatus Kering (< 40%) | 1 | |
| | Pompa Koreksi pH menyala otomatis saat pH tanah berstatus Asam (< 6) | 1 | |
| | **Jumlah** | **9** | **0** |

---

## UAT-04 — Kontrol Manual Aktuator dari Dashboard

| | |
|---|---|
| **Nama Sistem** | Sistem Monitoring dan Pengendalian Greenhouse Cabai Rawit Menggunakan Fuzzy Tahani Berbasis IoT |
| **Nomor Pengujian** | UAT-04 |
| **Topik Pengujian** | Pengendalian aktuator secara manual melalui dashboard web |
| **Tanggal Pengujian** | .............................................................. |
| **Penguji** | ➢ Bapak Ipat &nbsp;&nbsp; ➢ Bapak Rumadi &nbsp;&nbsp; ➢ Bapak Rohman |

| Penguji | Fungsi Pokok | Sesuai Ya | Sesuai Tidak |
|---------|-------------|-----------|--------------|
| | Switch ke mode Manual mengaktifkan tombol aktuator | 1 | |
| Bapak Ipat | Menekan tombol Kipas dan Kirim → Kipas menyala dari dashboard | 1 | |
| | Switch ke Otomatis mengembalikan sistem ke kontrol fuzzy | 1 | |
| | Switch ke mode Manual mengaktifkan tombol aktuator | 1 | |
| Bapak Rumadi | Menekan tombol Pompa Air dan Kirim → Pompa Air menyala dari dashboard | 1 | |
| | Switch ke Otomatis mengembalikan sistem ke kontrol fuzzy | 1 | |
| | Switch ke mode Manual mengaktifkan tombol aktuator | 1 | |
| Bapak Rohman | Menekan tombol Pompa pH dan Kirim → Pompa pH menyala dari dashboard | 1 | |
| | Switch ke Otomatis mengembalikan sistem ke kontrol fuzzy | 1 | |
| | **Jumlah** | **9** | **0** |

---

## UAT-05 — Riwayat Data dan Penyimpanan Database

| | |
|---|---|
| **Nama Sistem** | Sistem Monitoring dan Pengendalian Greenhouse Cabai Rawit Menggunakan Fuzzy Tahani Berbasis IoT |
| **Nomor Pengujian** | UAT-05 |
| **Topik Pengujian** | Tampilan riwayat data dari database Supabase pada dashboard |
| **Tanggal Pengujian** | .............................................................. |
| **Penguji** | ➢ Bapak Ipat &nbsp;&nbsp; ➢ Bapak Rumadi &nbsp;&nbsp; ➢ Bapak Rohman |

| Penguji | Fungsi Pokok | Sesuai Ya | Sesuai Tidak |
|---------|-------------|-----------|--------------|
| | Tabel riwayat menampilkan data sensor dan status relay historis | 1 | |
| Bapak Ipat | Data terbaru tampil di baris paling atas tabel | 1 | |
| | Tabel otomatis diperbarui setiap 30 detik tanpa reload | 1 | |
| | Tabel riwayat menampilkan data sensor dan status relay historis | 1 | |
| Bapak Rumadi | Data terbaru tampil di baris paling atas tabel | 1 | |
| | Tombol ↺ Muat ulang langsung memperbarui tabel | 1 | |
| | Tabel riwayat menampilkan data sensor dan status relay historis | 1 | |
| Bapak Rohman | Data terbaru tampil di baris paling atas tabel | 1 | |
| | Tombol ↺ Muat ulang langsung memperbarui tabel | 1 | |
| | **Jumlah** | **9** | **0** |

---

## Rekapitulasi Hasil UAT

| No | Nomor UAT | Topik Pengujian | Jumlah Ya | Jumlah Tidak |
|----|-----------|----------------|-----------|--------------|
| 1 | UAT-01 | Monitoring Sensor Real-Time | 12 | 0 |
| 2 | UAT-02 | Status Koneksi dan Informasi WiFi | 9 | 0 |
| 3 | UAT-03 | Pengendalian Otomatis Berbasis Fuzzy Tahani | 9 | 0 |
| 4 | UAT-04 | Kontrol Manual Aktuator dari Dashboard | 9 | 0 |
| 5 | UAT-05 | Riwayat Data dan Penyimpanan Database | 9 | 0 |
| | **Total** | | **48** | **0** |

---

## Kesimpulan

Berdasarkan hasil User Acceptance Test yang dilakukan oleh 3 responden
(Bapak Ipat, Bapak Rumadi, dan Bapak Rohman) terhadap 5 topik pengujian
dengan total 48 fungsi pokok, seluruh fungsi yang diuji mendapatkan
hasil **Sesuai (Ya = 48, Tidak = 0)**. Hal ini menunjukkan bahwa sistem
pertanian cerdas cabai rawit berbasis IoT dengan metode Fuzzy Tahani
yang telah dibangun dapat diterima oleh pengguna dan berjalan sesuai
dengan yang diharapkan.

---

*Dokumen UAT — Sistem Monitoring Greenhouse Cabai Rawit Berbasis IoT*
