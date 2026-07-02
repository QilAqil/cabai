# Panduan Presentasi
## Sistem Pertanian Cerdas Cabai Rawit Berbasis IoT dengan Metode Fuzzy Tahani

---

## 1. Pembuka (1–2 menit)

**Sampaikan:**
- Latar belakang: tanaman cabai rawit membutuhkan kondisi lingkungan yang
  terjaga — suhu, kelembaban tanah, dan pH tanah sangat mempengaruhi hasil panen
- Solusi: sistem monitoring dan pengendalian otomatis berbasis IoT menggunakan
  metode Fuzzy Tahani agar kondisi lingkungan selalu optimal tanpa harus
  dipantau terus-menerus

---

## 2. Komponen Sistem (2 menit)

**Hardware:**
| Komponen | Fungsi |
|----------|--------|
| ESP32 Dev Module | Otak sistem — membaca sensor, menjalankan fuzzy, mengirim data |
| DHT22 | Sensor suhu udara dan kelembaban udara |
| Soil Moisture | Sensor kelembaban tanah |
| Sensor pH + DMS | Sensor pH tanah dengan saklar daya |
| Relay × 3 | Mengendalikan kipas, pompa air, pompa pH |
| WiFi | Menghubungkan ke internet (MQTT + Supabase) |

**Software/Platform:**
- **EMQX Cloud** — broker MQTT untuk komunikasi real-time
- **Supabase** — database penyimpanan riwayat data
- **Dashboard Web** — tampilan monitoring di browser

---

## 3. Alur Kerja Sistem (3 menit)

**Jelaskan secara urut:**

```
1. Sensor membaca kondisi lingkungan setiap 15 detik
        ↓
2. Data dikirim ke ESP32 (suhu, kelembaban tanah, pH)
        ↓
3. Metode Fuzzy Tahani menghitung keputusan
        ↓
4. Relay aktuator menyala/mati sesuai keputusan
        ↓
5. Data dikirim ke MQTT (real-time) + Supabase (tiap 1 menit)
        ↓
6. Dashboard web menampilkan kondisi terkini dan riwayat
```

---

## 4. Metode Fuzzy Tahani (3 menit)

**Tiga variabel input:**
| Variabel | Himpunan | Kondisi Ideal Cabai |
|----------|----------|---------------------|
| Suhu Udara | Rendah / Sedang / Tinggi | 24–31°C |
| Kelembaban Tanah | Kering / Lembab / Basah | 50–70% |
| pH Tanah | Asam / Normal / Basa | pH 6–7 |

**Lima rule keputusan:**
| Rule | Kondisi | Aksi |
|------|---------|------|
| R1 | Suhu Tinggi (> 31°C) | Kipas Pendingin **ON** |
| R2 | Tanah Kering (< 40%) | Pompa Irigasi **ON** |
| R3 | Tanah Lembab AND Suhu Tinggi | Pompa Irigasi **ON** |
| R4 | pH Asam (< 6) | Pompa Koreksi pH **ON** (5 detik, jeda 2 jam) |

**Cara kerja singkat:**
- Setiap nilai sensor dihitung derajat keanggotaannya (0–1)
- Rule dievaluasi menggunakan operator AND (min) dan OR (max)
- Jika nilai melebihi threshold → aktuator menyala

---

## 5. Demo Dashboard (3 menit)

**Tunjukkan langsung di browser:**

1. **Header** — status WiFi (SSID, IP, sinyal), status MQTT
2. **Kartu sensor** — nilai suhu, kelembaban udara, kelembaban tanah, pH
   + badge status fuzzy (Rendah/Sedang/Tinggi dll)
3. **Kartu relay** — indikator ON/OFF kipas, pompa air, pompa pH
4. **Panel kontrol manual** — switch Otomatis ↔ Manual, tombol aktuator
5. **Tabel riwayat** — data historis dari Supabase, auto-refresh 30 detik

---

## 6. Hasil Pengujian (2 menit)

**Sampaikan:**
- Pengujian sensor pH: akurasi rata-rata **±95–99%** dibanding alat ukur manual
- Pengujian fuzzy: hasil perhitungan sistem **sesuai** dengan perhitungan manual (Excel)
- User Acceptance Test: 3 responden (Bapak Ipat, Bapak Rumadi, Bapak Rohman)
  → seluruh 48 fungsi yang diuji **Sesuai**

---

## 7. Demo Langsung (opsional, 3 menit)

**Skenario yang bisa ditunjukkan:**

1. **Tanah kering** → sensor soil membaca < 40% → pompa air menyala otomatis
2. **pH asam** → pH < 6 → pompa koreksi pH menyala 5 detik
3. **Kontrol manual** → geser switch ke Manual → klik Kipas → klik Kirim
   → kipas menyala langsung dari dashboard
4. **Tabel riwayat** → klik Muat Ulang → data terbaru langsung tampil

---

## 8. Kesimpulan (1 menit)

- Sistem berhasil memonitor dan mengendalikan kondisi greenhouse cabai rawit
  secara otomatis menggunakan metode Fuzzy Tahani
- Data tersimpan di cloud (Supabase) dan dapat diakses kapan saja melalui
  dashboard web
- Sistem responsif — perubahan kondisi dideteksi dan ditanggapi dalam
  hitungan detik

---

## Tips Presentasi

- **Tancapkan probe pH 5 menit sebelum presentasi** agar nilai sudah stabil
- Pastikan ESP32 sudah terhubung WiFi dan MQTT sebelum demo
- Siapkan larutan asam (cuka/air jeruk) untuk demo pompa pH menyala
- Buka dashboard di browser Chrome/Edge, zoom 80–90% agar tabel terlihat lengkap

---

*Panduan presentasi sistem pertanian cerdas cabai rawit berbasis IoT.*
