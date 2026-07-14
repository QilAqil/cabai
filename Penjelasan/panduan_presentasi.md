# Panduan Presentasi Sidang Tugas Akhir
## Sistem Pertanian Cerdas Tanaman Cabe Rawit
## Menggunakan Metode Fuzzy Tahani Berbasis IoT

---

## Persiapan Sebelum Sidang

| Checklist | Keterangan |
|-----------|------------|
| ☐ ESP32 menyala & terhubung WiFi `UPT-LAB-KOM` | Cek Serial Monitor: `[WiFi] OK — IP: ...` |
| ☐ MQTT terhubung ke broker EMQX | Cek: `[MQTT] Terhubung` di Serial |
| ☐ Probe pH ditancapkan **5 menit sebelum presentasi** | Butuh warmup 3 siklus (~75 detik) sebelum stabil |
| ☐ Dashboard web dibuka di Chrome/Edge, zoom 85% | URL: file lokal `index.html` atau GitHub Pages |
| ☐ Siapkan larutan asam (cuka/air jeruk) untuk demo pompa pH | pH target < 5.0 agar pompa menyala |
| ☐ Siapkan media tanah kering untuk demo pompa air | Kelembapan target < 40% |

---

## 1. Pembuka (1–2 menit)

**Sampaikan:**
- Cabai rawit memerlukan kondisi lingkungan yang presisi: **kelembapan tanah 50–70%**, **pH tanah 6–7**, **suhu udara 24–28°C**
- Pemantauan manual tidak efisien — petani harus hadir langsung ke greenhouse
- Solusi: sistem pertanian cerdas berbasis **ESP32 + FreeRTOS** dengan **Fuzzy Tahani** untuk keputusan otomatis dan **MQTT** untuk komunikasi real-time

---

## 2. Komponen Sistem (2 menit)

### Hardware

| Komponen | Pin ESP32 | Fungsi |
|----------|-----------|--------|
| ESP32 Dev Module | — | Mikrokontroler utama, dual-core FreeRTOS |
| DHT22 | GPIO 4 | Suhu udara & kelembapan udara |
| Soil Moisture Sensor | GPIO 35 (analog) | Kelembapan tanah (ADC 12-bit) |
| Sensor pH + Modul DMS | GPIO 34 (ADC) + GPIO 13 (DMS) | pH tanah — DMS mengatur daya elektroda |
| Relay 1-channel | GPIO 25 | Kipas pendingin |
| Relay 2-channel | GPIO 27, GPIO 26 | Pompa air (irigasi) & Pompa koreksi pH |
| LED indikator | GPIO 2 | Nyala saat sensor pH sedang baca |

### Software & Platform

| Platform | Fungsi |
|----------|--------|
| Arduino IDE + C++ | Pemrograman firmware ESP32 |
| EMQX Cloud | MQTT Broker TLS port 8883 / WSS 8084 |
| Supabase | Database cloud — REST API HTTPS |
| VS Code + HTML/CSS/JS | Dashboard web monitoring |
| GitHub Pages | Hosting dashboard |

---

## 3. Arsitektur FreeRTOS Dual-Core (2 menit)

**Jelaskan arsitektur ini sebagai keunggulan teknis:**

```
┌─────────────────────────────────────────────────────────────┐
│                        ESP32 Dual-Core                       │
├──────────────────────────┬──────────────────────────────────┤
│  Core 0 — TaskKomunikasi │  Core 1 — TaskSensor             │
│  (Prioritas 1)           │  (Prioritas 2)                   │
│                          │                                  │
│  • WiFi connect + NTP    │  • Baca DHT22 (suhu & RH)        │
│  • MQTT TLS keep-alive   │  • Baca Soil (50 sampel Kalman)  │
│  • Publish data 15 detik │  • Jeda isolasi galvanik 4 detik │
│  • Insert Supabase 60 dt │  • Baca pH (150 sampel + Kalman) │
│  • Terima perintah manual│  • Inferensi Fuzzy Tahani        │
│                          │  • tickPompa() setiap 20ms       │
│                          │  • Fast-path relay manual <20ms  │
├──────────────────────────┴──────────────────────────────────┤
│  Sinkronisasi: xSemaphoreMutex (SharedData)                 │
│  Notifikasi:   volatile flagDataBaru + volatile ManualCmd   │
└─────────────────────────────────────────────────────────────┘
```

**Poin penting untuk disampaikan:**
- Core 0 menangani jaringan — tidak terganggu saat sensor baca blocking
- Core 1 menangani sensor + fuzzy + relay — boleh `vTaskDelay` sepanjang apapun
- Mutex melindungi `SharedData` agar tidak race condition antar core
- Fast-path relay manual merespons < 20ms tanpa menunggu siklus sensor selesai

---

## 4. Alur Kerja Sistem (2 menit)

```
Setiap 15 detik:
  1. DHT22 → suhu udara & kelembapan udara
       ↓
  2. Soil Moisture (50 sampel, Kalman Filter)
       ↓
  3. Jeda isolasi galvanik 4 detik (anti-interferensi)
       ↓
  4. Sensor pH (150 sampel, Kalman + drift-guard + 3-titik kalibrasi)
       ↓
  5. Inferensi Fuzzy Tahani → keputusan relay
       ↓
  6. tickPompa() — eksekusi relay dengan logika timer:
     • Pompa Air: nyala 10 detik → jeda 30 menit
     • Pompa pH : nyala 10 detik → jeda 3 jam
     • Kipas    : langsung ON/OFF tanpa jeda

Setiap 15 detik juga:
  • Publish ke MQTT topic "pertanian/sensor" (JSON 768 byte)
  • Dashboard web menerima real-time via WebSocket

Setiap 60 detik:
  • Insert ke Supabase (REST API HTTPS)
```

---

## 5. Metode Fuzzy Tahani (3 menit)

### Variabel Input & Himpunan Fuzzy

| Variabel | Himpunan | Parameter | Fungsi Keanggotaan |
|----------|----------|-----------|-------------------|
| Suhu Udara | Rendah | (0, 0, 24, 27) | Bahu kiri (trapesium) |
| | Sedang | (24, 27, 31) | Segitiga |
| | Tinggi | (27, 31, 45, 45) | Bahu kanan (trapesium) |
| Kelembapan Tanah | Kering | (0, 0, 40, 50) | Bahu kiri |
| | Lembab | (40, 50, 70, 80) | Trapesium |
| | Basah | (70, 80, 100, 100) | Bahu kanan |
| pH Tanah | Asam | (3, 3, 5, 6) | Bahu kiri |
| | Normal | (5.5, 6, 7, 7.5) | Trapesium |
| | Basa | (7, 7.5, 9, 9) | Bahu kanan |

### Rule Base & Threshold

| Rule | Kondisi | Fire Strength | Threshold | Aktuator |
|------|---------|---------------|-----------|----------|
| R1 | IF Suhu **Tinggi** | μ_tinggi(T) | > **0.5** | 🌀 Kipas ON |
| R2 | IF Tanah **Kering** | μ_kering(S) | > **0.4** | 💦 Pompa Air ON |
| R3 | IF pH **Asam** | μ_asam(P) | > **0.4** | 🧪 Pompa pH ON |

> Catatan: Pompa hanya aktif jika nilai sensor valid (soil > 0, pH > 0) untuk mencegah aktuator menyala saat sensor tidak terpasang.

### Contoh Perhitungan Cepat

```
Input: Suhu = 29°C, Soil = 45%, pH = 6.90

μ_tinggi(29) = (29-27)/(31-27) = 0.50  → Kipas: 0.50 > 0.5? TIDAK
μ_kering(45) = (50-45)/(50-40) = 0.50  → Pompa Air: 0.50 > 0.4? YA
μ_asam(6.90) = 0.00                     → Pompa pH: 0.00 > 0.4? TIDAK

Hasil: Pompa Air ON (10 detik, lalu jeda 30 menit)
```

---

## 6. Inovasi Teknis Sensor pH (2 menit)

**Sampaikan sebagai keunggulan yang membedakan dari penelitian lain:**

### Masalah Umum Sensor pH Analog
- ADC elektroda pH sangat sensitif terhadap noise, suhu, dan interferensi galvanik
- Probe soil moisture meninggalkan muatan di media → menggeser baca pH

### Solusi yang Diimplementasikan

| Teknik | Parameter | Fungsi |
|--------|-----------|--------|
| Kalibrasi 3 titik | pH 4.01, 6.86, 9.18 | Akurasi interpolasi piecewise linear |
| 150 sampel ADC + trimmed mean 25% | `PH_SAMPLES=150`, `PH_TRIM_PCT=25` | Eliminasi outlier |
| Jeda isolasi galvanik | `GALVANIC_ISOLASI_MS=4000` | Anti-interferensi dari probe soil |
| Kalman Filter 1D | Q=0.001, R=0.40 | Stabilisasi pembacaan |
| Dead-band | 0.05 unit | Noise ADC residual |
| Drift-guard (arah) | 7 siklus, min 0.10/siklus | Cegah drift elektroda belum ekuilibrasi |
| Warmup 3 siklus | `PH_WARMUP_N=3` | Elektroda stabil sebelum output dipakai |
| DMS saklar daya | GPIO 13, ON 7 detik | Hemat umur elektroda |

**Hasil:** Akurasi rata-rata **94.12%** dibanding alat ukur manual

---

## 7. Demo Dashboard (3 menit)

**Tunjukkan langsung di browser — urutan demo:**

### 7.1 Header
- Dot hijau = MQTT terhubung ke EMQX
- WiFi: SSID + IP address ESP32 + bar kekuatan sinyal RSSI
- Timestamp update terakhir

### 7.2 Kartu Sensor (4 kartu kiri)
- **Suhu Udara** — nilai °C + badge Rendah/Sedang/Tinggi + bar derajat keanggotaan fuzzy
- **Kelembapan Udara** — nilai %RH dari DHT22
- **Kelembapan Tanah** — nilai % + badge Kering/Lembab/Basah + bar fuzzy
- **pH Tanah** — nilai pH + badge Asam/Normal/Basa + bar fuzzy

### 7.3 Kartu Relay (3 kartu kanan)
- 🌀 Kipas — ON (hijau) / OFF (abu)
- 💦 Pompa Irigasi — ON/OFF + countdown jeda (`⏳ Jeda 29:45` atau `✓ Siap`)
- 🧪 Pompa Koreksi pH — ON/OFF + countdown jeda

### 7.4 Panel Kontrol Manual
```
Switch Otomatis ↔ Manual
  → Manual aktif → tombol Kipas / Pompa Air / Pompa pH bisa diklik
  → Klik "Kirim ▶" → perintah dikirim via MQTT topic "pertanian/kontrol"
  → ESP32 merespons < 20ms (fast-path relay)
```

### 7.5 Tabel Riwayat Supabase
- 120 baris data terakhir, urut terbaru di atas
- Auto-refresh tiap 60 detik (countdown ditampilkan)
- Kolom: Waktu, T, RH, Tanah%, pH, F.Suhu, F.Tanah, F.PH, Kipas, Air, pH relay

---

## 8. Skenario Demo Langsung (opsional, 3 menit)

### Skenario A — Pompa Air Otomatis
1. Angkat sensor soil dari tanah (ADC mendekati 4095 → terdeteksi tidak terpasang → 0%)
   > Atau: cabut dan lap kering probe soil, tancapkan ke tanah sangat kering
2. Tunggu siklus sensor berikutnya (maks 15 detik)
3. Soil < 40% → μ_kering > 0.4 → **Pompa Air ON 10 detik**
4. Dashboard: kartu relay berubah hijau, countdown jeda mulai

### Skenario B — Pompa pH Otomatis
1. Celupkan probe pH ke larutan cuka/air jeruk (pH ~3–4)
2. Tunggu warmup 3 siklus jika probe baru ditancapkan (~75 detik total)
3. pH < 5.0 → μ_asam > 0.4 → **Pompa pH ON 10 detik**
4. Dashboard: kartu relay pH berubah hijau

### Skenario C — Kontrol Manual
1. Geser switch **Otomatis → Manual**
2. Badge berubah kuning "MANUAL"
3. Klik tombol **🌀 Kipas** → tombol highlight hijau
4. Klik **Kirim ▶** → kipas menyala langsung < 20ms
5. Geser kembali ke **Otomatis** → sistem kembali ke kendali Fuzzy

### Skenario D — Riwayat Data
1. Klik **↺ Muat ulang**
2. Data terbaru dari Supabase muncul dengan animasi highlight biru
3. Tunjukkan nilai fuzzy (F.Suhu, F.Tanah, F.PH) di kolom tabel

---

## 9. Hasil Pengujian (2 menit)

### Akurasi Sensor

| Sensor | Metode Pengujian | Hasil |
|--------|-----------------|-------|
| DHT22 (Suhu) | Bandingkan Serial Monitor vs Website (5 kali) | **100% sesuai** — nilai identik |
| Soil Moisture | Bandingkan Serial Monitor vs Website (5 kali) | **100% sesuai** — nilai identik |
| pH Tanah | Bandingkan vs buffer standar (asam/normal/basa, 5 kali tiap kondisi) | **Rata-rata 94.12%** |

### Pengujian Fuzzy Tahani

| No | pH | Suhu | Tanah | Manual vs Sistem | Sesuai? |
|----|-----|------|-------|-----------------|---------|
| 1 | 6.90 | 22.0 | 70% | 1.00/1.00/1.00 vs 1.00/1.00/1.00 | ✅ Ya |
| 2 | 6.90 | 22.0 | 71% | 1.00/1.00/0.90 vs 1.00/1.00/0.90 | ✅ Ya |
| 3 | 6.43 | 23.1 | 58% | 1.00/1.00/1.00 vs 1.00/1.00/1.00 | ✅ Ya |
| 4 | 6.31 | 23.1 | 58% | 1.00/1.00/1.00 vs 1.00/1.00/1.00 | ✅ Ya |
| 5 | 5.95 | 24.4 | 49% | 0.90/0.87/0.90 vs 0.90/0.87/0.90 | ✅ Ya |

**Hasil: 5/5 sesuai — perhitungan sistem identik dengan perhitungan Excel manual**

### User Acceptance Test (UAT)

| Skenario | Responden | Sesuai | Tidak |
|----------|-----------|--------|-------|
| UAT-01A: Pembacaan sensor di dashboard | Bapak Ipat, Rumadi, Rohman | 9 | 0 |
| UAT-02A: Kontrol manual via dashboard | Bapak Ipat, Rumadi, Rohman | 9 | 0 |
| UAT-03A: Pengendalian otomatis Fuzzy Tahani | Bapak Ipat, Rumadi, Rohman | 9 | 0 |
| **TOTAL** | | **27** | **0** |

**Tingkat kesesuaian: 27/(27+0) × 100 = 100%**

---

## 10. Kesimpulan (1 menit)

1. **Sistem IoT ESP32 (FreeRTOS dual-core)** berhasil memantau suhu, kelembapan tanah, dan pH tanah secara real-time melalui dashboard web
2. **Metode Fuzzy Tahani** berhasil mengolah 3 variabel sensor menjadi keputusan pengendalian aktuator otomatis
3. **Protokol MQTT** memastikan data terkirim real-time dengan latensi rendah — relay manual merespons < 20ms
4. Seluruh sensor, aktuator, dan fitur berfungsi sesuai rancangan — **UAT 100%**
5. Akurasi sensor pH rata-rata **94.12%**, pertumbuhan tanaman lebih stabil setelah sistem diterapkan

---

## 11. Antisipasi Pertanyaan Penguji

**Q: Mengapa memilih Fuzzy Tahani, bukan Mamdani atau Sugeno?**
> Fuzzy Tahani berbasis database relasional — cocok untuk query multi-variabel lingkungan tanaman yang tersimpan di Supabase. Metode ini memungkinkan evaluasi kondisi secara fleksibel menggunakan fire strength, bukan defuzzifikasi numerik, sehingga lebih efisien di mikrokontroler.

**Q: Mengapa ada jeda 4 detik antara baca soil dan baca pH?**
> Probe soil moisture meninggalkan muatan kapasitif di media tanam setelah pengukuran. Tanpa jeda, muatan ini menggeser potensial elektroda pH dan menghasilkan pembacaan yang salah (interferensi galvanik). Jeda 4 detik memberikan waktu muatan tersebut terurai.

**Q: Mengapa menggunakan Kalman Filter untuk sensor?**
> Sensor ADC pada ESP32 memiliki noise ±10–30 LSB. Kalman Filter 1D dengan parameter Q=0.001/R=0.40 (pH) dan Q=0.0005/R=2.00 (soil) menstabilkan output tanpa lag berlebihan — lebih presisi dari moving average biasa.

**Q: Bagaimana jika WiFi/MQTT terputus?**
> Sistem memiliki mekanisme reconnect otomatis di `taskKomunikasi`. Sensor dan relay tetap bekerja normal di `taskSensor` karena berjalan di core terpisah. Data tersimpan di Supabase setiap 60 detik sebagai backup.

**Q: Mengapa pompa memiliki jeda 30 menit / 3 jam?**
> Tanaman cabai tidak memerlukan irigasi terus-menerus. Jeda 30 menit untuk pompa air mencegah over-watering. Jeda 3 jam untuk pompa pH mencegah perubahan pH yang terlalu agresif yang bisa merusak mikroorganisme tanah.

**Q: Akurasi pH 94.12% — apakah cukup untuk pertanian?**
> Untuk monitoring pertanian skala greenhouse kecil, toleransi ±0.3–0.5 pH unit sudah memadai untuk mendeteksi kondisi asam/normal/basa. Sensor pH profesional dengan akurasi lebih tinggi bisa digunakan untuk pengembangan selanjutnya.

---

## Tips Presentasi

- **Jangan buka Serial Monitor** saat demo — angka bergerak cepat bisa membingungkan
- **Tancapkan semua probe** sebelum masuk ruang sidang
- Jika dashboard menampilkan `–` (belum ada data), tunggu siklus sensor berikutnya (maks 15 detik)
- Jika MQTT terputus, dot merah akan muncul — jelaskan bahwa reconnect otomatis sedang berjalan
- pH menampilkan `0.00` saat warmup — ini normal, terjadi pada 3 siklus pertama (~45–75 detik)
- Buka dashboard di tab terpisah dari PPT agar mudah switch saat demo

---

*Panduan ini dibuat berdasarkan kode aktual `index/index.ino` (FreeRTOS dual-core) dan `index.html` (MQTT.js + Supabase REST).*
