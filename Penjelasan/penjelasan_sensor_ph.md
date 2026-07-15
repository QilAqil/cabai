# Pengkodean ESP32 dengan Sensor pH Tanah

---

## 1. Deklarasi Pin

```cpp
const int PH_ADC_PIN = 34;
const int DMS_PIN    = 13;
const int LED_PIN    = 2;
```

**`PH_ADC_PIN` (GPIO34)**
Pin input analog untuk membaca tegangan keluaran dari modul sensor pH tanah.
GPIO34 dipilih karena bersifat *input-only* dan mendukung ADC pada ESP32.

**`DMS_PIN` (GPIO13)**
Pin output digital untuk mengaktifkan dan menonaktifkan daya ke modul sensor
pH melalui saklar daya (DMS). Sensor dimatikan saat tidak dipakai untuk
memperpanjang umur elektroda probe.

**`LED_PIN` (GPIO2)**
Pin output LED built-in ESP32 sebagai indikator visual bahwa proses pembacaan
pH sedang berlangsung.

---

## 2. Interval Waktu

```cpp
const unsigned long INTERVAL_PH_ON  = 7000;
const unsigned long INTERVAL_PH_OFF = 2000;
```

**`INTERVAL_PH_ON` (7000 ms)**
Durasi DMS aktif sebelum ADC dibaca. Memberikan waktu 7 detik agar tegangan
keluaran elektroda sensor stabil setelah daya dihidupkan. Selama periode ini
`tickPompa()` tetap dipanggil setiap 100ms sehingga relay manual tetap responsif.

**`INTERVAL_PH_OFF` (2000 ms)**
Jeda setelah DMS dimatikan sebelum program melanjutkan, memberi waktu rangkaian
melepas muatan residual. Juga diselingi `tickPompa()` tiap 100ms.

---

## 3. Tabel Kalibrasi 3 Titik

```cpp
struct PhCalPoint { int adc; float ph; };
const PhCalPoint PH_CAL[3] = {
  { 1082, 4.01f },  // buffer pH 4.01
  {  689, 6.86f },  // buffer pH 6.86
  {  520, 9.18f },  // buffer pH 9.18 (estimasi — wajib diukur ulang)
};
```

Kalibrasi menggunakan **3 titik larutan buffer standar** untuk menghasilkan
persamaan piecewise linear yang lebih akurat dibanding regresi 1 titik.
Setiap pasang `{adc, ph}` diperoleh dari:
1. Celupkan probe ke larutan buffer
2. Tunggu 5–10 menit hingga ADC stabil
3. Catat nilai `ADCtrim` dari Serial Monitor
4. Isi ke tabel sesuai nilai pH buffer yang digunakan

> **Catatan:** Titik pH 9.18 masih estimasi proporsional dan wajib diverifikasi
> dengan larutan buffer nyata sebelum sistem digunakan untuk pengambilan data.

---

## 4. Parameter Filter

```cpp
const int   PH_SAMPLES       = 150;
const int   PH_TRIM_PCT      = 25;
const float PH_NOISE_GATE    = 1.20f;
const float PH_TEMP_COEF     = 0.003f;
const float PH_TEMP_REF      = 25.0f;
const int   PH_ADC_BATAS     = 1250;
const int   PH_NOTANCAP_CNT  = 5;
const int   PH_VARIANSI_MAX  = 150;
const int   PH_WARMUP_N      = 3;
const int   PH_DRIFT_MIN     = 8;
const int   PH_DRIFT_CNT_MAX = 3;
const float PH_KALMAN_Q      = 0.001f;
const float PH_KALMAN_R      = 0.40f;
const float PH_DEADBAND      = 0.05f;
const int   PH_DRIFT_ARAH_MAX  = 7;
const float PH_DRIFT_ARAH_MIN  = 0.10f;
```

| Parameter | Nilai | Fungsi |
|-----------|-------|--------|
| `PH_SAMPLES` | 150 | Jumlah sampel ADC per siklus |
| `PH_TRIM_PCT` | 25% | Buang 25% sampel atas & bawah sebelum rata-rata |
| `PH_NOISE_GATE` | 1.20 | Tolak pembacaan jika loncat > 1.2 unit pH dalam satu siklus |
| `PH_ADC_BATAS` | 1250 | ADC ≥ 1250 → probe dianggap tidak terpasang |
| `PH_NOTANCAP_CNT` | 5 | Konfirmasi 5 siklus berturut sebelum dinyatakan tidak terpasang |
| `PH_VARIANSI_MAX` | 150 | ADC terlalu berisik jika rentang (5%–95%) > 150 |
| `PH_WARMUP_N` | 3 | Siklus warmup: siklus 1–2 dibuang, siklus 3 langsung output |
| `PH_DRIFT_MIN` | 8 | Kenaikan ADC minimum per siklus yang dihitung sebagai drift naik |
| `PH_DRIFT_CNT_MAX` | 3 | Reset setelah 3 siklus drift naik berturut-turut |
| `PH_KALMAN_Q` | 0.001 | Process noise Kalman: pH larutan berubah sangat lambat |
| `PH_KALMAN_R` | 0.40 | Measurement noise Kalman: skeptis terhadap ADC analog |
| `PH_DEADBAND` | 0.05 | Output tidak bergerak jika perubahan Kalman < 0.05 unit |
| `PH_DRIFT_ARAH_MAX` | 7 | Freeze output setelah 7 siklus bergerak searah (~105 detik) |
| `PH_DRIFT_ARAH_MIN` | 0.10 | Perubahan < 0.10/siklus tidak dihitung sebagai drift arah |

---

## 5. Fungsi Konversi ADC ke pH

```cpp
static float adcToPH(int adc) { ... }
```

Menggunakan **interpolasi piecewise linear** dari 3 titik kalibrasi.
Titik kalibrasi diurutkan terlebih dahulu berdasarkan nilai ADC ascending,
lalu nilai pH dihitung dengan interpolasi linear antar segmen:

```
Segmen 1: ADC 520–689  → pH 9.18–6.86
Segmen 2: ADC 689–1082 → pH 6.86–4.01
Di luar rentang        → ekstrapolasi dari segmen terdekat
```

Lebih akurat dari regresi linier global karena kurva elektroda pH tidak
linier di seluruh rentang 4–9.

---

## 6. Fungsi Kompensasi Suhu

```cpp
static float kompensasiSuhu(float ph, float suhu) {
  return ph - (PH_TEMP_COEF * (suhu - PH_TEMP_REF) * (ph - 7.0f));
}
```

Tegangan elektroda pH berubah sesuai hukum Nernst terhadap suhu. Semakin
jauh dari suhu referensi (25°C) dan semakin jauh dari pH netral (7.0),
koreksi semakin besar. Contoh untuk suhu 28°C dan pH 5.45:

```
dT      = 28 - 25 = 3
koreksi = 0.003 × 3 × (5.45 - 7.0) = -0.014
pH_true = 5.45 - (-0.014) = 5.464
```

---

## 7. Fungsi Utama bacaPH — Pipeline 10 Tahap

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
for (unsigned long elapsed = 0; elapsed < INTERVAL_PH_ON; elapsed += 100) {
  tickPompa();
  vTaskDelay(pdMS_TO_TICKS(100));
}
```
DMS aktif LOW (LOW = ON). `tickPompa()` dipanggil setiap 100ms selama
menunggu 7 detik agar perintah relay manual dari dashboard tetap diproses
tanpa tertunda.

---

### Tahap 3: Sampling 150 Titik
```cpp
for (int i = 0; i < PH_SAMPLES; i++) {
  s[i] = analogRead(PH_ADC_PIN);
  vTaskDelay(pdMS_TO_TICKS(5));
}
```
150 sampel dengan jeda 5ms = total 750ms sampling.

---

### Tahap 4: Matikan DMS, Jeda OFF
```cpp
digitalWrite(DMS_PIN, HIGH);
digitalWrite(LED_PIN, LOW);
for (unsigned long elapsed = 0; elapsed < INTERVAL_PH_OFF; elapsed += 100) {
  tickPompa();
  vTaskDelay(pdMS_TO_TICKS(100));
}
```
DMS dimatikan, lalu jeda 2 detik sambil tetap menjalankan `tickPompa()`.

---

### Tahap 5: Validasi Variansi + Trimmed Mean
```cpp
sortArray(s, PH_SAMPLES);
int variansi = s[PH_SAMPLES-1-varN] - s[varN];  // rentang 5%–95%
if (variansi > PH_VARIANSI_MAX) { phResetState(); return 0.0f; }

int adcTrim = rata-rata s[37]–s[112];  // buang 25% atas & bawah
```
Jika variansi melebihi 150, data dianggap tidak stabil (misal interferensi
galvanik dari sensor soil) dan fungsi mengembalikan 0.0f setelah reset state.
Trimmed mean 25% membuang outlier hardware sebelum konversi ke pH.

---

### Tahap 6: Deteksi Probe Tidak Terpasang
```cpp
if (adcFinal >= PH_ADC_BATAS) {
  phNotTancapCnt++;
  if (phNotTancapCnt >= PH_NOTANCAP_CNT) {
    phResetState(); return 0.0f;
  }
  return phLast;
}
```
ADC ≥ 1250 menandakan probe di udara. Dikonfirmasi 5 siklus berturut-turut
untuk menghindari false detection akibat fluktuasi ADC sesaat.

---

### Tahap 7: Deteksi Drift Naik ADC
```cpp
if (adcPrevious > 0 && adcFinal > adcPrevious + PH_DRIFT_MIN) {
  if (++phDriftCount >= PH_DRIFT_CNT_MAX) {
    phResetState(); return 0.0f;
  }
}
```
Jika ADC naik > 8 poin selama 3 siklus berturut-turut, elektroda dianggap
mengering dan state di-reset. `adcPrevious` juga di-reset bersama state
sehingga tidak terjadi false-trigger setelah probe dilepas dan dipasang ulang.

---

### Tahap 8: Konversi, Kompensasi, Noise Gate
```cpp
float phRaw  = adcToPH(adcFinal);
float phComp = kompensasiSuhu(phRaw, suhuLokal);

if (phLast > 0.0f && fabsf(phComp - phLast) > PH_NOISE_GATE)
  { phResetState(); return 0.0f; }
```
ADC dikonversi ke pH via interpolasi piecewise, dikoreksi suhu, lalu
diperiksa noise gate 1.20 unit. Jika melewati batas, state di-reset dan
fungsi mengembalikan 0.0f (bukan nilai lama) agar fuzzy tidak mengambil
keputusan dari data yang tidak valid.

---

### Tahap 9: Warmup Kalman
```cpp
if (phWarmupCount < PH_WARMUP_N) {
  phWarmupCount++;
  kfEst = phComp; kfP = 1.0f; phLast = phComp;
  if (phWarmupCount < PH_WARMUP_N) return 0.0f;
  return phComp;  // siklus warmup terakhir langsung output
}
```
Tiga siklus warmup untuk memberi estimator Kalman nilai awal yang akurat.
Siklus 1–2 dibuang (return 0.0f), siklus ke-3 langsung menghasilkan output
pertama tanpa menunggu konvergensi lebih lanjut.

---

### Tahap 10: Kalman Filter 1D + Dead-band + Deteksi Drift Arah

```cpp
// Kalman update
kfP += PH_KALMAN_Q;
float K     = kfP / (kfP + PH_KALMAN_R);
float kfNew = kfEst + K * (phComp - kfEst);
kfP         = (1.0f - K) * kfP;
if (fabsf(kfNew - kfEst) >= PH_DEADBAND) kfEst = kfNew;

// Deteksi drift arah
float deltaKf = kfEst - phLast;
if (fabsf(deltaKf) >= PH_DRIFT_ARAH_MIN) {
  // hitung siklus bergerak searah
} else {
  phDriftArahCnt = 0;  // stabil, reset hitungan
}

// Freeze jika drift searah melewati batas
float phOutput = kfEst;
if (abs(phDriftArahCnt) >= PH_DRIFT_ARAH_MAX)
  phOutput = phDriftArahRef;
```

| Mekanisme | Fungsi |
|-----------|--------|
| Kalman Filter 1D | Timbang pengukuran baru vs estimasi berdasarkan noise relatif |
| Dead-band 0.05 | Abaikan perubahan < 0.05 unit (noise ADC residual) |
| Drift arah freeze | Freeze output jika pH bergerak searah > 7 siklus (~105 detik) |

**Cara kerja Kalman:**
- Saat baru konvergen (P besar) → K besar → ikuti pengukuran baru
- Saat sudah stabil (P kecil) → K kecil → tahan noise, output halus
- Q=0.001 mencerminkan pH larutan yang sangat stabil antar siklus
- R=0.40 mencerminkan ketidakpercayaan terhadap ADC analog yang noisy

**Cara kerja drift arah freeze:**
Elektroda pH analog mengalami drift gradual saat belum ekuilibrasi dengan
larutan. Jika output bergerak searah > 7 siklus dengan perubahan ≥ 0.10/siklus,
output di-freeze ke nilai referensi awal drift. Kalman internal tetap berjalan
sehingga saat arah berbalik (elektroda mulai stabil), freeze otomatis lepas.

---

## 8. Urutan Pembacaan dalam Siklus Sensor

```
bacaPH() → jeda isolasi galvanik 4 detik → bacaSoil()
```

pH dibaca **lebih dulu** dari soil untuk menghindari interferensi galvanik.
Saat DMS aktif, modul pH mengalirkan arus referensi kecil melalui elektroda
ke larutan. Jika probe soil ikut terendam di medium yang sama selama periode
ini, hambatan yang terukur sensor soil akan bergeser.

Urutan ini memastikan:
1. DMS sudah OFF saat soil mulai diukur
2. Jeda 4 detik memberi waktu potensial sisa elektroda pH terurai di medium
3. Sensor soil membaca hambatan dalam kondisi medium yang netral

---

## 9. Cross-Contamination Guard

```cpp
if (soilVal > 80.0f && deltaPH > 1.0f) {
  phBaru = 0.0f; phResetState();   // interferensi galvanik
} else if (deltaPH > 1.5f) {
  phBaru = 0.0f; phResetState();   // spike ADC
}
```

Lapisan pertahanan terakhir setelah jeda isolasi. Jika:
- Soil sangat basah (>80%) DAN pH lompat >1.0 → tolak, reset ke 0
- pH lompat >1.5 tanpa syarat → tolak, reset ke 0

Nilai 0.0f dikembalikan (bukan nilai lama) agar fuzzy mengenali kondisi
"data tidak valid" dan tidak mengaktifkan pompa pH berdasarkan data palsu.

---

## 10. Output Serial Monitor

```
[pH] Warmup 1/3 ADC=1082 phRaw=4.010 phComp=4.024
[pH] Warmup 2/3 ADC=1080 phRaw=4.017 phComp=4.031
[pH] Warmup 3/3 ADC=1081 phRaw=4.013 phComp=4.027
[pH] ADCtrim=1081 var=12 phRaw=4.013 phComp=4.027 K=0.002 kfEst=4.025 phOut=4.025 driftCnt=0
[pH] Drift arah turun 7 siklus → freeze di 4.025
```

| Field | Keterangan |
|-------|-----------|
| `ADCtrim` | Nilai ADC hasil trimmed mean 25% |
| `var` | Variansi ADC (rentang 5%–95% dari 150 sampel) |
| `phRaw` | pH sebelum kompensasi suhu |
| `phComp` | pH setelah kompensasi suhu |
| `K` | Kalman Gain siklus ini (kecil = filter stabil) |
| `kfEst` | Estimasi internal Kalman |
| `phOut` | Nilai pH akhir yang dipakai fuzzy |
| `driftCnt` | Hitungan drift arah (positif = turun, negatif = naik) |

---

## 11. Pengiriman Data

```cpp
doc["ph"]       = snap.pH;        // MQTT + Supabase
doc["adc_ph"]   = snap.adcPH;     // MQTT saja (monitoring)
doc["fuzzy_ph"] = snap.mu_pompa_ph; // derajat keanggotaan Asam (0.0–1.0)
```

**`ph`** — Nilai pH akhir (output `bacaPH()`) dikirim ke MQTT dan disimpan
ke Supabase setiap 60 detik atau segera saat relay berubah state.

**`adc_ph`** — ADC mentah hasil trimmed mean, dikirim via MQTT untuk
keperluan monitoring dan kalibrasi ulang.

**`fuzzy_ph`** — Derajat keanggotaan himpunan "Asam" dari inferensi Fuzzy
Tahani. Nilai 0.0 = tidak asam, 1.0 = sangat asam. Disimpan ke kolom
`fuzzy_ph` di Supabase sebagai rekam jejak keputusan sistem.

---

*File ini menjelaskan implementasi pembacaan sensor pH tanah pada sistem
monitoring greenhouse cabai rawit berbasis ESP32 dengan arsitektur FreeRTOS
dual-core.*
