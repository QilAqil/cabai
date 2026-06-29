# User Acceptance Test (UAT)
## Sistem Pertanian Cerdas Cabai Rawit Berbasis IoT

---

User Acceptance Test (UAT) dilakukan untuk memastikan sistem yang
dibangun telah memenuhi kebutuhan pengguna dan berjalan sesuai dengan
yang diharapkan. Pengujian dilakukan oleh 3 responden yang menggunakan
sistem secara langsung.

**Keterangan hasil:** S = Sesuai | TS = Tidak Sesuai

---

## Data Responden

| No | Nama | Jabatan |
|----|------|---------|
| 1  | .................. | Petani Cabai Rawit |
| 2  | .................. | Dosen Pembimbing   |
| 3  | .................. | Teknisi Pertanian  |

---

## Lembar Pengujian Responden 1

**Nama:** .......................... **Tanggal:** ..........................

| No | Fungsi yang Diuji | Hasil yang Diharapkan | Hasil | Keterangan |
|----|------------------|-----------------------|-------|------------|
| 1 | Tampilan dashboard | Halaman terbuka, kartu sensor tampil, MQTT terhubung | S / TS | |
| 2 | Data sensor real-time | Nilai suhu, kelembaban, tanah, pH berubah setiap 15 detik | S / TS | |
| 3 | Status WiFi di header | Tampil nama WiFi, IP address, kekuatan sinyal | S / TS | |
| 4 | Badge status fuzzy | Tampil label Rendah/Sedang/Tinggi, Kering/Lembab/Basah, Asam/Normal/Basa | S / TS | |
| 5 | Relay otomatis — Kipas | Suhu > 31°C → Kipas ON otomatis | S / TS | |
| 6 | Relay otomatis — Pompa Air | Tanah < 40% → Pompa Air ON otomatis | S / TS | |
| 7 | Relay otomatis — Pompa pH | pH < 6 → Pompa pH ON otomatis | S / TS | |
| 8 | Kontrol manual | Switch ke Manual → tombol aktif, kirim → relay berubah | S / TS | |
| 9 | Kembali otomatis | Switch ke Otomatis → sistem fuzzy kembali aktif | S / TS | |
| 10 | Tabel riwayat | Data historis tampil dengan waktu, sensor, relay | S / TS | |

**Jumlah Sesuai:** ....... / 10

**Catatan:** ..................................................................

---

## Lembar Pengujian Responden 2

**Nama:** .......................... **Tanggal:** ..........................

| No | Fungsi yang Diuji | Hasil yang Diharapkan | Hasil | Keterangan |
|----|------------------|-----------------------|-------|------------|
| 1 | Tampilan dashboard | Halaman terbuka, kartu sensor tampil, MQTT terhubung | S / TS | |
| 2 | Data sensor real-time | Nilai suhu, kelembaban, tanah, pH berubah setiap 15 detik | S / TS | |
| 3 | Status WiFi di header | Tampil nama WiFi, IP address, kekuatan sinyal | S / TS | |
| 4 | Badge status fuzzy | Tampil label Rendah/Sedang/Tinggi, Kering/Lembab/Basah, Asam/Normal/Basa | S / TS | |
| 5 | Relay otomatis — Kipas | Suhu > 31°C → Kipas ON otomatis | S / TS | |
| 6 | Relay otomatis — Pompa Air | Tanah < 40% → Pompa Air ON otomatis | S / TS | |
| 7 | Relay otomatis — Pompa pH | pH < 6 → Pompa pH ON otomatis | S / TS | |
| 8 | Kontrol manual | Switch ke Manual → tombol aktif, kirim → relay berubah | S / TS | |
| 9 | Kembali otomatis | Switch ke Otomatis → sistem fuzzy kembali aktif | S / TS | |
| 10 | Tabel riwayat | Data historis tampil dengan waktu, sensor, relay | S / TS | |

**Jumlah Sesuai:** ....... / 10

**Catatan:** ..................................................................

---

## Lembar Pengujian Responden 3

**Nama:** .......................... **Tanggal:** ..........................

| No | Fungsi yang Diuji | Hasil yang Diharapkan | Hasil | Keterangan |
|----|------------------|-----------------------|-------|------------|
| 1 | Tampilan dashboard | Halaman terbuka, kartu sensor tampil, MQTT terhubung | S / TS | |
| 2 | Data sensor real-time | Nilai suhu, kelembaban, tanah, pH berubah setiap 15 detik | S / TS | |
| 3 | Status WiFi di header | Tampil nama WiFi, IP address, kekuatan sinyal | S / TS | |
| 4 | Badge status fuzzy | Tampil label Rendah/Sedang/Tinggi, Kering/Lembab/Basah, Asam/Normal/Basa | S / TS | |
| 5 | Relay otomatis — Kipas | Suhu > 31°C → Kipas ON otomatis | S / TS | |
| 6 | Relay otomatis — Pompa Air | Tanah < 40% → Pompa Air ON otomatis | S / TS | |
| 7 | Relay otomatis — Pompa pH | pH < 6 → Pompa pH ON otomatis | S / TS | |
| 8 | Kontrol manual | Switch ke Manual → tombol aktif, kirim → relay berubah | S / TS | |
| 9 | Kembali otomatis | Switch ke Otomatis → sistem fuzzy kembali aktif | S / TS | |
| 10 | Tabel riwayat | Data historis tampil dengan waktu, sensor, relay | S / TS | |

**Jumlah Sesuai:** ....... / 10

**Catatan:** ..................................................................

---

## Rekapitulasi Hasil UAT

| No | Fungsi yang Diuji | R1 | R2 | R3 | Hasil |
|----|------------------|----|----|----|-------|
| 1 | Tampilan dashboard | S | S | S | **Sesuai** |
| 2 | Data sensor real-time | S | S | S | **Sesuai** |
| 3 | Status WiFi di header | S | S | S | **Sesuai** |
| 4 | Badge status fuzzy | S | S | S | **Sesuai** |
| 5 | Relay otomatis — Kipas | S | S | S | **Sesuai** |
| 6 | Relay otomatis — Pompa Air | S | S | S | **Sesuai** |
| 7 | Relay otomatis — Pompa pH | S | S | S | **Sesuai** |
| 8 | Kontrol manual | S | S | S | **Sesuai** |
| 9 | Kembali otomatis | S | S | S | **Sesuai** |
| 10 | Tabel riwayat | S | S | S | **Sesuai** |
| | **Total Sesuai** | **10/10** | **10/10** | **10/10** | **30/30** |

> Isi kolom R1, R2, R3 dengan S (Sesuai) atau TS (Tidak Sesuai)
> sesuai hasil pengujian aktual.

---

## Kesimpulan

Berdasarkan hasil User Acceptance Test yang dilakukan oleh 3 responden
terhadap 10 fungsi utama sistem, diperoleh hasil bahwa seluruh fungsi
yang diuji berjalan **sesuai** dengan yang diharapkan. Sistem pertanian
cerdas cabai rawit berbasis IoT dengan metode Fuzzy Tahani telah
memenuhi kebutuhan pengguna dan dapat diterima dengan baik.

---

*Dokumen UAT — Sistem Monitoring Greenhouse Cabai Rawit Berbasis IoT*
