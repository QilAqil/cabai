# Pengkodean ESP32 dengan Sensor pH Tanah

---

## 1. Deklarasi Pin

```cpp
const int PH_ADC_PIN = 34;
const int DMS_PIN    = 13;
const int LED_PIN    = 2;
```

**`PH_ADC_PIN` (GPIO34)**
Pin input analog untuk membaca tegangan keluaran dari modul sensor
pH tanah. GPIO34 dipilih karena bersifat *input-only* dan mendukung
ADC pada ESP32, sehingga tidak bisa tidak sengaja diset sebagai output.

**`DMS_PIN` (GPIO13)**
Pin output digital untuk mengaktifkan dan menonaktifkan daya ke
modul sensor pH melalui saklar daya (DMS). Sensor dimatikan saat
tidak dipakai untuk memperpanjang umur elektroda probe.

**`LED_PIN` (GPIO2)**
Pin output LED built-in ESP32 sebagai indikator visual bahwa proses
pembacaan pH sedang berlangsung.

---

## 2. Interval Waktu

```cpp
const unsigned long INTERVAL_PH_ON  = 5000UL;
const unsigned long INTERVAL_PH_OFF = 2000UL;
```

**`INTERVAL_PH_ON` (5000 ms)**
Durasi DMS aktif sebelum ADC dibaca. Memberikan waktu agar tegangan
keluaran elektroda sensor stabil setelah daya dihidupkan, sehingga
pembacaan tidak terpengaruh lonjakan tegangan awal.

**`INTERVAL_PH_OFF` (2000 ms)**
Jeda setelah DMS dimatikan sebelum program melanjutkan ke proses
berikutnya, memberi waktu rangkaian melepas muatan residual.

---

## 3. Tabel Kalibrasi 3 Titik

```cpp
struct PhCalPoint { int adc; float ph; };
const PhCalPoint PH_CAL[3] = {
  { 1062, 4.01f },  // buffer pH 4.01
  {  804, 6.86f },  // buffer pH 6.86
  {  600, 9.18f },  // buffer pH 9.18
};
```

Kalibrasi menggunakan **3 titik larutan buffer standar** untuk
menghasilkan persamaan yang lebih akurat dibanding 1 titik.
Setiap pasang `{adc, ph}` diperoleh dari:
1. Celupkan probe ke larutan buffer
2. Tunggu 3–5 menit hingga ADC stabil
3. Catat nilai `ADCfinal` dari Serial Monitor
4. Isi ke tabel sesuai nilai pH buffer yang digunakan

---

## 4. Parameter Filter

```cpp
const int   PH_SAMPLES    = 150;    // jumlah sampel ADC
const int   PH_TRIM_PCT   = 25;     // buang 25% atas & bawah
const int   PH_MA_SIZE    = 20;     // moving average 20 siklus
const int   PH_MICRO_SIZE = 5;      // micro-buffer 5 siklus
const float PH_EMA_ALPHA  = 0.05f;  // bobot EMA
const float PH_NOISE_GATE = 0.80f;  // batas noise gate
const float PH_RATE_LIMIT = 0.005f; // max perubahan per siklus
const float PH_HYSTERESIS = 0.05f;  // batas hysteresis
const float PH_TEMP_COEF  = 0.003f; // koefisien kompensasi suhu
const int   PH_ADC_BATAS  = 1080;   // threshold probe tidak tancap
const int   PH_NOTANCAP_CNT = 5;    // konfirmasi tidak tancap
```

| Parameter | Nilai | Fungsi |
|-----------|-------|--------|
| `PH_SAMPLES` | 150 | Sampel lebih banyak → lebih akurat |
| `PH_TRIM_PCT` | 25% | Buang outlier atas & bawah |
| `PH_MA_SIZE` | 20 | Rata-rata 20 siklus (~5 menit) |
| `PH_EMA_ALPHA` | 0.05 | Smoothing sangat halus |
| `PH_NOISE_GATE` | 0.80 | Lonjakan >0.8 pH diabaikan |
| `PH_RATE_LIMIT` | 0.005 | Max 0.005 pH berubah per siklus |
| `PH_HYSTERESIS` | 0.05 | Perubahan <0.05 tidak ditampilkan |
| `PH_ADC_BATAS` | 1080 | ADC ≥ 1080 → probe tidak terpasang |
| `PH_NOTANCAP_CNT` | 5 | Konfirmasi 5x sebelum dianggap tidak tancap |

---

## 5. Fungsi Konversi ADC ke pH

```cpp
static float adcToPH(int adc) { ... }
```

Menggunakan **interpolasi piecewise** dari 3 titik kalibrasi.
Titik kalibrasi diurutkan terlebih dahulu berdasarkan ADC, lalu
nilai pH dihitung dengan interpolasi linear antar segmen:

```
Segmen 1: ADC 600–804   → pH 9.18–6.86
Segmen 2: ADC 804–1062  → pH 6.86–4.01
Di luar rentang → ekstrapolasi dari segmen terdekat
```

Lebih akurat dari regresi linier global karena kurva elektroda
pH sebenarnya tidak linier di seluruh rentang.

---

## 6. Fungsi Kompensasi Suhu

```cpp
static float kompensasiSuhu(float ph, float suhu) {
  return ph - (PH_TEMP_COEF * (suhu - PH_TEMP_REF) * (ph - 7.0f));
}
```

Tegangan elektroda pH berubah sesuai hukum Nernst terhadap suhu.
Semakin jauh dari suhu referensi (25°C) dan semakin jauh dari pH 7,
koreksi semakin besar. Contoh untuk suhu 28°C dan pH 5.45:

```
dT      = 28 - 25 = 3
koreksi = 0.003 × 3 × (5.45 - 7) = 0.003 × 3 × (-1.55) = -0.014
pH_true = 5.45 - (-0.014) = 5.464
```

---

## 7. Fungsi Utama bacaPH — Pipeline 11 Tahap

### Tahap 1: Setup ADC
```cpp
analogReadResolution(12);
analogSetAttenuation(ADC_11db);
```
Resolusi 12-bit (0–4095) dan attenuation 11dB (range 0–3.9V) sesuai
tegangan output modul pH yang bekerja pada 0–3.3V.

---

### Tahap 2: Nyalakan DMS, Tunggu Stabil
```cpp
digitalWrite(DMS_PIN, LOW);
digitalWrite(LED_PIN, HIGH);
while (millis() - dmsStart < INTERVAL_PH_ON) {
  mqttClient.loop();
  delay(500);
}
```
DMS aktif LOW (LOW = ON). `mqttClient.loop()` dipanggil setiap 500ms
selama menunggu agar perintah kontrol manual dari dashboard tetap
dapat diproses tanpa delay 5 detik.

---

### Tahap 3: Sampling 150 Titik
```cpp
for (int i = 0; i < PH_SAMPLES; i++) {
  s[i] = analogRead(PH_ADC_PIN);
  delay(5);
  if (i % 10 == 0) mqttClient.loop();
}
```
150 sampel dengan jeda 5ms = total 750ms sampling. `mqttClient.loop()`
setiap 10 sampel agar MQTT tetap responsif selama proses sampling.

---

### Tahap 4: Sort, Median, Trimmed Mean
```cpp
sortArray(s, PH_SAMPLES);
int adcMed  = (s[74] + s[75]) / 2;        // median
int adcTrim = rata-rata s[37]–s[112];     // buang 25% atas & bawah
int adcFinal = (int)(adcTrim*0.60f + adcMed*0.40f);
```
Median dan trimmed mean digabungkan dengan bobot 60:40 untuk
mendapatkan nilai ADC yang paling representatif dan tahan noise.

---

### Tahap 5: Deteksi Probe Tidak Terpasang
```cpp
if (adcFinal >= PH_ADC_BATAS) {
  phNotTancapCount++;
  if (phNotTancapCount >= PH_NOTANCAP_CNT) {
    return 0.0f;  // pH = 0 setelah 5 konfirmasi
  }
  return phLast;  // belum 5x → pakai nilai terakhir
}
phNotTancapCount = 0;
```
ADC ≥ 1080 menandakan probe di udara (bias op-amp tanpa beban).
Dikonfirmasi 5 siklus berturut-turut untuk menghindari false detection
akibat fluktuasi ADC sesaat.

---

### Tahap 6–7: Konversi dan Kompensasi
```cpp
float phRaw  = adcToPH(adcFinal);
float phComp = kompensasiSuhu(phRaw, g_suhu);
```
ADC final dikonversi ke pH via interpolasi piecewise, lalu
dikoreksi terhadap suhu menggunakan persamaan Nernst.

---

### Tahap 8: Noise Gate
```cpp
if (phLast > 0.0f &&
    fabsf(phComp - phLast) > PH_NOISE_GATE) {
  return phLast;
}
```
Jika perubahan lebih dari 0.80 pH dalam satu siklus, pembacaan
dianggap lonjakan tidak valid dan nilai terakhir dipertahankan.

---

### Tahap 9–11: Multi-layer Smoothing

```cpp
// Micro-buffer 5 siklus
phMicro[phMicro_idx] = phComp;
phMV = rata-rata phMicro;

// Weighted Moving Average 20 siklus
phMA[phMA_idx] = phMV;
phMAv = weighted average phMA (bobot linier, terbaru lebih besar);

// EMA alpha=0.05
phEMA = 0.05*phMAv + 0.95*phEMA;

// Rate limiter + hysteresis
phOutput += max ±0.005 per siklus jika perubahan > 0.05
```

| Filter | Tujuan |
|--------|--------|
| Micro-buffer 5 siklus | Haluskan lonjakan antar pembacaan |
| Weighted MA 20 siklus | Rata-rata jangka menengah (~5 menit) |
| EMA 0.05 | Smoothing akhir sangat halus |
| Rate limiter 0.005/siklus | Output berubah maksimal 0.005 per 15 detik |
| Hysteresis 0.05 | Perubahan kecil tidak ditampilkan |

---

## 8. Output Serial Monitor

```
[pH] ADCtrim=964 ADCmed=966 ADCfinal=965
     phRaw=4.487 phComp=4.490 phMicro=4.491
     phMA=4.492 phEMA=4.492 phOut=4.492
```

Menampilkan seluruh tahapan pipeline dari ADC mentah hingga nilai
pH akhir untuk memudahkan monitoring dan kalibrasi.

---

## 9. Pengiriman Data

```cpp
doc["ph"]        = g_pH;
doc["adc_ph"]    = g_adcPH;
doc["fuzzy_ph"]  = fo.mu_pompa_ph;
```

**`ph`** — Nilai pH akhir dikirim ke MQTT dan Supabase.
**`adc_ph`** — ADC mentah dikirim via MQTT untuk monitoring.
**`fuzzy_ph`** — Derajat keanggotaan Asam (0.0–1.0) hasil inferensi
fuzzy, disimpan ke kolom `fuzzy_ph` di Supabase.

---

*File ini menjelaskan implementasi pembacaan sensor pH tanah
pada sistem monitoring greenhouse cabai rawit berbasis ESP32.*
