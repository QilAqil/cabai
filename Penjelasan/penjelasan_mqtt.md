# Pengkodean ESP32 — Pengiriman Data ke MQTT (EMQX Cloud)

---

## 1. Library yang Digunakan

```cpp
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
```

**`WiFiClientSecure`**
Library untuk koneksi TCP berbasis TLS/SSL. Digunakan karena broker
EMQX Cloud menggunakan port 8883 yang mengharuskan enkripsi data
agar tidak bisa disadap selama transmisi melalui internet.

**`PubSubClient`**
Library implementasi protokol MQTT untuk ESP32. Menangani koneksi,
subscribe, dan publish ke broker MQTT secara otomatis.

**`ArduinoJson`**
Library untuk membuat dan memproses format JSON. Data sensor dikemas
dalam format JSON sebelum dikirim ke broker MQTT agar mudah diproses
oleh dashboard web.

---

## 2. Konfigurasi Broker MQTT

```cpp
const char* MQTT_SERVER = "n01d3130.ala.asia-southeast1.emqxsl.com";
const int   MQTT_PORT   = 8883;
const char* MQTT_USER   = "pertanian";
const char* MQTT_PASS   = "pertanian12";
const char* TOPIC_PUB   = "pertanian/sensor";
const char* TOPIC_SUB   = "pertanian/kontrol";
```

**`MQTT_SERVER`**
Alamat hostname broker EMQX Cloud yang dituju ESP32 untuk koneksi
MQTT. Broker ini berjalan di server cloud wilayah Asia Tenggara.

**`MQTT_PORT` (8883)**
Port standar MQTT over TLS. Berbeda dari port 1883 (MQTT tanpa
enkripsi), port 8883 memastikan semua data terenkripsi selama
pengiriman dari ESP32 ke broker.

**`MQTT_USER` dan `MQTT_PASS`**
Kredensial autentikasi ke broker. EMQX Cloud memerlukan username
dan password agar hanya klien yang diizinkan bisa terhubung.

**`TOPIC_PUB` ("pertanian/sensor")**
Topik MQTT tempat ESP32 mempublikasikan data sensor setiap 15 detik.
Format topik menggunakan slash sebagai pemisah hierarki.

**`TOPIC_SUB` ("pertanian/kontrol")**
Topik MQTT yang disubscribe ESP32 untuk menerima perintah kontrol
manual dari dashboard web.

---

## 3. Inisialisasi Objek MQTT

```cpp
WiFiClientSecure tlsClient;
PubSubClient     mqttClient(tlsClient);
```

**`tlsClient`**
Objek koneksi TLS yang dikhususkan untuk MQTT. Dibuat terpisah dari
koneksi HTTP Supabase agar tidak terjadi konflik koneksi (error -5).

**`mqttClient(tlsClient)`**
Membuat objek MQTT client yang menggunakan `tlsClient` sebagai
transport layer, sehingga semua komunikasi MQTT berjalan di atas
enkripsi TLS.

---

## 4. Fungsi Koneksi MQTT

```cpp
void koneksiMQTT() {
  tlsClient.setInsecure();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(640);

  String clientId = "ESP32-Cabai-" +
    String((uint32_t)ESP.getEfuseMac(), HEX);

  while (!mqttClient.connected()) {
    if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
      mqttClient.subscribe(TOPIC_SUB);
    } else {
      delay(3000);
    }
  }
}
```

**`tlsClient.setInsecure()`**
Menonaktifkan verifikasi sertifikat CA sehingga ESP32 tetap bisa
terhubung ke broker tanpa menyimpan sertifikat root. Cukup untuk
lingkungan pengembangan dan penelitian.

**`mqttClient.setServer()`**
Mendaftarkan alamat dan port broker ke objek MQTT client.

**`mqttClient.setCallback(mqttCallback)`**
Mendaftarkan fungsi yang dipanggil setiap ada pesan masuk dari broker.
Digunakan untuk menerima perintah kontrol manual dari dashboard.

**`mqttClient.setBufferSize(640)`**
Mengatur ukuran buffer MQTT menjadi 640 byte sesuai ukuran payload
JSON yang dikirim. Jika terlalu kecil, pesan akan terpotong.

**`ESP.getEfuseMac()`**
Mengambil MAC address unik dari chip ESP32 untuk membentuk `clientId`
yang unik, mencegah konflik jika ada beberapa perangkat terhubung.

**`mqttClient.connect()`**
Mengirimkan paket CONNECT ke broker dengan clientId, username, dan
password untuk autentikasi.

**`mqttClient.subscribe(TOPIC_SUB)`**
Mendaftarkan diri untuk menerima pesan dari topik kontrol setelah
koneksi berhasil.

---

## 5. Fungsi Publish Data Sensor

```cpp
void publishMQTT(const FuzzyOutput& fo) {
  if (!mqttClient.connected()) return;

  StaticJsonDocument<640> doc;
  doc["temperature"]  = g_suhu;
  doc["humidity"]     = g_kelembaban;
  doc["soil"]         = g_soil;
  doc["ph"]           = g_pH;
  doc["fuzzy_suhu"]   = fo.mu_kipas;
  doc["fuzzy_soil"]   = fo.mu_pompa_air;
  doc["fuzzy_ph"]     = fo.mu_pompa_ph;
  doc["relay_kipas"]  = relay_kipas_state     ? 1.0f : 0.0f;
  doc["relay_air"]    = relay_pompa_air_state ? 1.0f : 0.0f;
  doc["relay_ph"]     = relay_pompa_ph_state  ? 1.0f : 0.0f;
  doc["status_suhu"]  = fo.status_suhu;
  doc["status_tanah"] = fo.status_tanah;
  doc["status_ph"]    = fo.status_ph;
  doc["adc_ph"]       = g_adcPH;
  doc["adc_soil"]     = g_adcSoil;
  doc["waktu"]        = getWaktu();
  doc["manual_mode"]  = manual_mode;
  doc["wifi_ssid"]    = WiFi.SSID();
  doc["wifi_ip"]      = WiFi.localIP().toString();
  doc["wifi_rssi"]    = WiFi.RSSI();
  doc["wifi_status"]  = (WiFi.status() == WL_CONNECTED)
                        ? "Terhubung" : "Terputus";

  char buf[640];
  serializeJson(doc, buf);
  mqttClient.setBufferSize(640);
  mqttClient.publish(TOPIC_PUB, buf, true);
}
```

**`if (!mqttClient.connected()) return`**
Keluar dari fungsi jika koneksi MQTT sedang terputus, mencegah
error saat mencoba mengirim data tanpa koneksi.

**`StaticJsonDocument<640>`**
Membuat dokumen JSON di stack memori dengan kapasitas 640 byte.
`Static` dipilih agar tidak menggunakan heap dan menghindari
fragmentasi memori pada ESP32.

**Field data yang dikirim:**

| Field | Isi | Keterangan |
|-------|-----|------------|
| `temperature` | g_suhu | Suhu udara °C dari DHT22 |
| `humidity` | g_kelembaban | Kelembaban udara %RH dari DHT22 |
| `soil` | g_soil | Kelembaban tanah % dari soil sensor |
| `ph` | g_pH | Nilai pH tanah dari probe pH |
| `fuzzy_suhu` | fo.mu_kipas | Derajat keanggotaan suhu (0.0–1.0) |
| `fuzzy_soil` | fo.mu_pompa_air | Derajat keanggotaan tanah (0.0–1.0) |
| `fuzzy_ph` | fo.mu_pompa_ph | Derajat keanggotaan pH (0.0–1.0) |
| `relay_kipas` | 0.0/1.0 | Status kipas OFF/ON |
| `relay_air` | 0.0/1.0 | Status pompa air OFF/ON |
| `relay_ph` | 0.0/1.0 | Status pompa pH OFF/ON |
| `status_suhu` | "Rendah/Sedang/Tinggi" | Label himpunan fuzzy suhu |
| `status_tanah` | "Kering/Lembab/Basah" | Label himpunan fuzzy tanah |
| `status_ph` | "Asam/Normal/Basa" | Label himpunan fuzzy pH |
| `waktu` | "HH:MM:SS" | Timestamp WIB dari NTP |
| `manual_mode` | true/false | Status mode kontrol |
| `wifi_ssid` | nama WiFi | SSID jaringan terhubung |
| `wifi_ip` | IP address | Alamat IP ESP32 |
| `wifi_rssi` | dBm | Kekuatan sinyal WiFi |

**`serializeJson(doc, buf)`**
Mengubah objek JSON menjadi string karakter siap kirim.

**`mqttClient.publish(TOPIC_PUB, buf, true)`**
Mengirimkan payload JSON ke topik `pertanian/sensor`. Parameter
ketiga `true` mengaktifkan **retained message** — broker menyimpan
pesan terakhir dan langsung mengirimkannya ke subscriber baru
yang baru terhubung, sehingga dashboard langsung menampilkan data
terkini saat pertama dibuka.

---

## 6. Callback MQTT (Terima Perintah Manual)

```cpp
void mqttCallback(char* topic, byte* payload,
                  unsigned int length) {
  StaticJsonDocument<200> doc;
  if (deserializeJson(doc, payload, length)) return;

  if (doc.containsKey("manual"))
    manual_mode = doc["manual"].as<bool>();

  if (manual_mode) {
    if (doc.containsKey("kipas"))
      manual_kipas     = doc["kipas"].as<bool>();
    if (doc.containsKey("pompa_air"))
      manual_pompa_air = doc["pompa_air"].as<bool>();
    if (doc.containsKey("pompa_ph"))
      manual_pompa_ph  = doc["pompa_ph"].as<bool>();
    terapkanAktuator(manual_kipas,
                     manual_pompa_air, manual_pompa_ph);
    FuzzyOutput fo = inferensiFuzzy(g_suhu, g_soil, g_pH);
    publishMQTT(fo);  // kirim feedback segera ke dashboard
  }
}
```

**`deserializeJson()`**
Mengurai string JSON dari payload MQTT menjadi objek yang bisa
diakses. Jika format tidak valid, fungsi langsung keluar.

**`terapkanAktuator()`**
Diterapkan **langsung** di dalam callback tanpa menunggu siklus
berikutnya, sehingga relay merespons dalam < 1 detik setelah
tombol ditekan di dashboard.

**`publishMQTT(fo)`**
Mengirimkan feedback status relay terbaru ke dashboard segera
setelah perubahan, memperbarui tampilan indikator relay secara
real-time.

---

## 7. Menjaga Koneksi di Loop Utama

```cpp
void loop() {
  if (!mqttClient.connected()) koneksiMQTT();
  mqttClient.loop();
  ...
}
```

**`mqttClient.connected()`**
Memeriksa apakah koneksi ke broker masih aktif setiap iterasi.
Jika terputus, `koneksiMQTT()` dipanggil untuk reconnect otomatis.

**`mqttClient.loop()`**
Memproses paket MQTT yang masuk dan menjaga koneksi tetap hidup
dengan mengirimkan paket PINGREQ ke broker secara berkala.
Juga dipanggil di dalam `bacaPH()` setiap 10 sampel agar
perintah manual tidak tertunda selama proses sampling.

---

## 8. Interval Pengiriman

```cpp
const unsigned long INTERVAL_BACA = 15000UL;  // 15 detik
```

Data sensor dikirim ke MQTT setiap **15 detik** bersamaan dengan
pembacaan sensor. Supabase menerima data setiap **60 detik**
(lebih jarang untuk menghemat kuota API).

---

*File ini menjelaskan implementasi pengiriman data ke broker MQTT
EMQX Cloud pada sistem monitoring greenhouse cabai rawit berbasis ESP32.*
