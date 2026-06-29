# Pengkodean ESP32 dengan Sensor Suhu dan Kelembaban (DHT22)

---

## 1. Library

```cpp
#include <DHT.h>
```

Menyertakan library Adafruit DHT untuk komunikasi dengan sensor DHT22
melalui protokol single-wire. Library ini menangani seluruh proses
pengiriman dan penerimaan data dari sensor secara otomatis.
Diinstall melalui Arduino Library Manager dengan nama
**"DHT sensor library"** by Adafruit.

---

## 2. Deklarasi Pin dan Tipe Sensor

```cpp
#define DHTPIN  4
#define DHTTYPE DHT22
```

**`DHTPIN` (GPIO4)**
Pin data digital yang menghubungkan ESP32 dengan sensor DHT22.
Komunikasi menggunakan satu kabel yang membawa data dua arah
secara bergantian (protokol 1-Wire).

**`DHTTYPE DHT22`**
Mendefinisikan tipe sensor sebagai DHT22 (AM2302). Dipilih karena
akurasi lebih baik dari DHT11 dengan rentang:
- Suhu   : −40°C hingga +80°C (akurasi ±0.5°C)
- Kelembaban : 0–100%RH (akurasi ±2%)

---

## 3. Inisialisasi Objek Sensor

```cpp
DHT dht(DHTPIN, DHTTYPE);
```

Membuat objek `dht` dari kelas `DHT` dengan parameter pin GPIO4
dan tipe sensor DHT22. Objek ini yang digunakan untuk memanggil
semua fungsi pembacaan sensor.

---

## 4. Variabel Penyimpan Hasil Baca

```cpp
float g_suhu       = 0.0f;
float g_kelembaban = 0.0f;
```

**`g_suhu`**
Variabel global bertipe `float` untuk menyimpan nilai suhu udara
dalam derajat Celsius. Diawali 0.0 sebagai nilai fallback jika
sensor belum terbaca.

**`g_kelembaban`**
Variabel global untuk menyimpan nilai kelembaban relatif udara
dalam persen (%RH). Dikirim ke MQTT dan Supabase sebagai data
monitoring tambahan meski tidak diproses oleh fuzzy.

---

## 5. Inisialisasi di setup()

```cpp
dht.begin();
```

Menginisialisasi komunikasi dengan sensor DHT22 di dalam `setup()`.
Fungsi ini mengatur timing protokol 1-Wire sesuai spesifikasi sensor
dan harus dipanggil satu kali sebelum fungsi baca digunakan.

---

## 6. Pembacaan Sensor

```cpp
float t = dht.readTemperature();
float h = dht.readHumidity();
```

**`dht.readTemperature()`**
Mengirimkan sinyal permintaan ke sensor, menunggu respons data
40-bit (5 byte) yang berisi nilai suhu, kelembaban, dan checksum,
lalu mengekstrak nilai suhu dalam Celsius.

**`dht.readHumidity()`**
Membaca nilai kelembaban relatif udara dari paket data 40-bit
yang sama dengan pembacaan suhu, sehingga keduanya menggunakan
satu kali komunikasi dengan sensor.

---

## 7. Validasi Hasil Baca

```cpp
if (!isnan(t) && !isnan(h)) {
    g_suhu = t;
    g_kelembaban = h;
}
```

**`isnan()`**
Memeriksa apakah nilai yang dikembalikan sensor adalah
*Not a Number* (NaN). DHT22 mengembalikan NaN jika komunikasi
gagal akibat:
- Kabel data lepas atau longgar
- Sensor rusak atau belum siap
- Gangguan timing komunikasi

Jika pembacaan valid, nilai diperbarui ke variabel global.
Jika gagal, **nilai terakhir yang valid tetap dipertahankan**
agar sistem tidak berhenti atau mengirim data kosong ke MQTT
dan Supabase.

---

## 8. Tampilan di Serial Monitor

```cpp
Serial.printf("[%s] Suhu:%.1fC Hum:%.1f%%...\n",
  getWaktu().c_str(), g_suhu, g_kelembaban, ...);
```

Menampilkan nilai suhu dan kelembaban ke Serial Monitor
disertai timestamp WIB dari NTP. Contoh output:

```
[13:25:32] Suhu:24.4C Hum:73.1% SoilADC:1832 Soil:65.0% pHADC:964 pH:4.50 | Kipas:OFF Air:OFF pH:ON [AUTO]
```

---

## 9. Pengiriman Data ke MQTT dan Supabase

```cpp
doc["temperature"] = g_suhu;
doc["humidity"]    = g_kelembaban;
doc["fuzzy_suhu"]  = fo.mu_kipas;
```

**`temperature`**
Nilai suhu dikirim ke broker MQTT (EMQX Cloud) dan diinsert ke
tabel Supabase dengan nama kolom `temperature` bertipe `float4`.

**`humidity`**
Nilai kelembaban udara dikirim sebagai data monitoring tambahan
dengan nama kolom `humidity` di Supabase.

**`fuzzy_suhu`**
Derajat keanggotaan himpunan *Tinggi* dari nilai suhu hasil
inferensi fuzzy (0.0 – 1.0). Disimpan ke kolom `fuzzy_suhu`
di Supabase untuk analisis historis keputusan sistem.

---

## 10. Penggunaan Nilai Suhu pada Fuzzy

Nilai `g_suhu` diproses oleh 3 fungsi keanggotaan:

```cpp
static float fuzzyTempRendah(float x) { ... }  // trapmf(0, 0, 24, 27)
static float fuzzyTempSedang(float x) { ... }  // trimf(24, 27, 31)
static float fuzzyTempTinggi(float x) { ... }  // trapmf(27, 31, 45, 45)
```

| Himpunan | Bentuk | Rentang Penuh | Aksi |
|----------|--------|---------------|------|
| Rendah   | Trapesium kiri  | ≤ 24°C | Kipas OFF |
| Sedang   | Segitiga        | 24–31°C | Transisi |
| Tinggi   | Trapesium kanan | ≥ 31°C | Kipas ON |

Nilai suhu juga digunakan pada Rule 3 Pompa Air:

```
R3: IF tanah Lembab AND suhu Tinggi → POMPA AIR ON
    Operator AND = min(μ_Lembab, μ_Tinggi)
```

---

*File ini menjelaskan implementasi pembacaan sensor suhu DHT22
pada sistem monitoring greenhouse cabai rawit berbasis ESP32.*
