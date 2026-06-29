# Pengkodean ESP32 dengan Sensor Kelembaban Tanah

---

## 1. Deklarasi Pin

```cpp
const int SOIL_PIN = 35;
```

**`SOIL_PIN` (GPIO35)**
Pin input analog untuk membaca tegangan keluaran dari pin AO
(*Analog Output*) modul sensor kelembaban tanah. GPIO35 dipilih
karena mendukung ADC dan bersifat *input-only* pada ESP32,
sehingga tidak bisa tidak sengaja diset sebagai output.

> **Catatan:** Sensor kelembaban tanah memiliki dua output:
> - **AO (Analog Output)** → digunakan, memberi nilai 0–4095
> - **DO (Digital Output)** → tidak digunakan, hanya ON/OFF
>
> Sistem fuzzy membutuhkan nilai kontinu (persentase), sehingga
> hanya pin AO yang dihubungkan ke ESP32.

---

## 2. Kalibrasi ADC

```cpp
const int SOIL_ADC_KERING = 3800;   // ADC saat kering → 0%
const int SOIL_ADC_BASAH  =  800;   // ADC saat basah  → 100%
```

**`SOIL_ADC_KERING` (3800)**
Nilai ADC referensi saat probe sensor berada di udara bebas
(kondisi kering 0%). Semakin kering, resistansi antar probe
semakin tinggi, tegangan output semakin besar, ADC semakin besar.

**`SOIL_ADC_BASAH` (800)**
Nilai ADC referensi saat probe sensor tercelup ke dalam air
(kondisi basah 100%). Semakin lembab, resistansi menurun,
tegangan output menurun, ADC semakin kecil.

**Hubungan terbalik:** ADC besar = kering, ADC kecil = basah.

### Cara Kalibrasi Manual

```
Langkah 1: Cabut probe dari tanah, biarkan di udara 30 detik
           → Catat nilai SoilADC di Serial Monitor
           → Isi ke SOIL_ADC_KERING

Langkah 2: Celupkan ujung probe ke dalam air 30 detik
           → Catat nilai SoilADC di Serial Monitor
           → Isi ke SOIL_ADC_BASAH
```

---

## 3. Variabel Global

```cpp
float g_soil    = 0.0f;
int   g_adcSoil = 0;
```

**`g_soil`**
Variabel global menyimpan hasil perhitungan kelembaban tanah
dalam satuan persen (0–100%). Nilai ini yang diproses oleh
fungsi keanggotaan fuzzy.

**`g_adcSoil`**
Variabel global menyimpan nilai ADC mentah hasil pembacaan
GPIO35 (0–4095). Ditampilkan di Serial Monitor dan dikirim
via MQTT untuk keperluan monitoring dan kalibrasi ulang.

---

## 4. Fungsi Pembacaan Sensor

```cpp
float bacaSoil() {
  analogReadResolution(12);
  int raw = analogRead(SOIL_PIN);
  g_adcSoil = raw;
  float pct = map(raw, SOIL_ADC_KERING, SOIL_ADC_BASAH, 0, 100);
  return constrain(pct, 0.0f, 100.0f);
}
```

**`analogReadResolution(12)`**
Mengatur resolusi ADC menjadi 12-bit sehingga menghasilkan nilai
integer antara **0 hingga 4095**. Resolusi 12-bit digunakan pada
sensor kelembaban tanah agar rentang pembacaan lebih lebar dan
lebih presisi dibanding sensor pH yang memakai 10-bit.

**`analogRead(SOIL_PIN)`**
Membaca tegangan analog dari pin AO sensor pada GPIO35. Semakin
lembab tanah, semakin rendah resistansi antar probe, semakin
rendah tegangan output, semakin kecil nilai ADC yang terbaca.

**`g_adcSoil = raw`**
Menyimpan nilai ADC mentah ke variabel global agar dapat
ditampilkan di Serial Monitor dan dikirim melalui MQTT.

**`map(raw, SOIL_ADC_KERING, SOIL_ADC_BASAH, 0, 100)`**
Memetakan nilai ADC mentah ke rentang persentase 0–100%:
- ADC = 3800 (kering) → dipetakan ke **0%**
- ADC = 800  (basah)  → dipetakan ke **100%**

Karena hubungan terbalik (ADC besar = kering), nilai kering
diletakkan di posisi 0 dan nilai basah di posisi 100.

**`constrain(pct, 0.0f, 100.0f)`**
Membatasi hasil pemetaan agar tidak melampaui rentang 0–100%.
Mencegah nilai negatif atau lebih dari 100% jika ADC terbaca
di luar batas kalibrasi karena kondisi ekstrem.

---

## 5. Pemanggilan di Loop Utama

```cpp
g_soil = bacaSoil();
```

Dipanggil setiap `INTERVAL_BACA` (15 detik). Hasilnya disimpan
ke `g_soil` yang kemudian digunakan untuk inferensi fuzzy,
ditampilkan di Serial Monitor, dikirim ke MQTT, dan diinsert
ke Supabase.

---

## 6. Tampilan di Serial Monitor

```cpp
Serial.printf("[%s] ... SoilADC:%d Soil:%.1f%% ...\n",
  getWaktu().c_str(), g_adcSoil, g_soil, ...);
```

Contoh output:
```
[13:25:32] Suhu:24.4C Hum:73.1% SoilADC:1832 Soil:65.0% pHADC:964 pH:4.50 | Kipas:OFF Air:OFF pH:ON [AUTO]
```

- `SoilADC:1832` → nilai ADC mentah dari GPIO35
- `Soil:65.0%`   → hasil konversi ke persentase kelembaban

---

## 7. Pengiriman Data ke MQTT dan Supabase

```cpp
doc["soil"]       = g_soil;
doc["adc_soil"]   = g_adcSoil;
doc["fuzzy_soil"] = fo.mu_pompa_air;
```

**`soil`**
Nilai kelembaban tanah dalam persen dikirim ke broker MQTT dan
diinsert ke tabel Supabase dengan nama kolom `soil` bertipe `float4`.

**`adc_soil`**
Nilai ADC mentah dikirim via MQTT untuk keperluan monitoring
dan kalibrasi ulang di kemudian hari.

**`fuzzy_soil`**
Derajat keanggotaan dominan hasil inferensi fuzzy untuk parameter
kelembaban tanah (0.0–1.0), disimpan ke kolom `fuzzy_soil`
di Supabase.

---

## 8. Penggunaan Nilai Tanah pada Fuzzy

Nilai `g_soil` diproses oleh 3 fungsi keanggotaan:

```cpp
static float fuzzySoilKering(float x) { ... }  // trapmf(0, 0, 40, 50)
static float fuzzySoilLembab(float x) { ... }  // trapmf(40, 50, 70, 80)
static float fuzzySoilBasah (float x) { ... }  // trapmf(70, 80, 100, 100)
```

| Himpunan | Bentuk | Rentang Penuh | Aksi |
|----------|--------|---------------|------|
| Kering | Trapesium kiri  | ≤ 40% | Pompa Air ON |
| Lembab | Trapesium tengah | 50–70% | Pompa Air ON jika suhu Tinggi |
| Basah  | Trapesium kanan  | ≥ 80% | Pompa Air OFF |

Digunakan pada 2 rule inferensi:

```
R2: IF tanah KERING → POMPA AIR ON
    μ_PompaAir = μ_Kering

R3: IF tanah LEMBAB AND suhu TINGGI → POMPA AIR ON
    μ_PompaAir = min(μ_Lembab, μ_Tinggi)   ← AND = ambil terkecil

Hasil akhir: μ_PompaAir = max(R2, R3)      ← OR = ambil terbesar
```

Pompa air menyala jika `μ_PompaAir > 0.4`.

---

*File ini menjelaskan implementasi pembacaan sensor kelembaban tanah
pada sistem monitoring greenhouse cabai rawit berbasis ESP32.*
