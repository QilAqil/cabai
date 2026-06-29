# Pengkodean ESP32 — Pengiriman Data ke Database (Supabase)

---

## 1. Library yang Digunakan

```cpp
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
```

**`WiFiClientSecure`**
Library koneksi TCP berbasis TLS/SSL. Diperlukan karena endpoint
Supabase menggunakan HTTPS (port 443) yang mengharuskan semua
komunikasi terenkripsi.

**`HTTPClient`**
Library untuk melakukan HTTP request (GET, POST, PUT) dari ESP32.
Digunakan untuk mengirim data ke Supabase REST API dengan metode
HTTP POST.

**`ArduinoJson`**
Library untuk membuat format JSON. Data sensor dikemas dalam JSON
sebelum dikirim ke Supabase agar sesuai dengan format yang diterima
oleh REST API.

---

## 2. Konfigurasi Supabase

```cpp
#define SUPABASE_URL  "https://sptomqebtvclfebaktof.supabase.co"
#define SUPABASE_KEY  "sb_publishable_jqEF4hY0nK0Bu0BkKK2ayQ_eW_8fh_u"
#define TABLE_NAME    "pertanian"
```

**`SUPABASE_URL`**
Alamat URL project Supabase. Setiap project memiliki subdomain unik
yang menjadi endpoint REST API untuk operasi database PostgreSQL.

**`SUPABASE_KEY`**
API key (anon key) Supabase untuk autentikasi request. Key ini
disisipkan di setiap HTTP request agar Supabase memverifikasi bahwa
request berasal dari klien yang diizinkan.

**`TABLE_NAME` ("pertanian")**
Nama tabel di database PostgreSQL Supabase tempat data sensor
disimpan. Tabel ini memiliki kolom:

| Kolom | Tipe | Keterangan |
|-------|------|------------|
| id | int8 | Primary key, auto-increment |
| temperature | float4 | Suhu udara °C |
| humidity | float4 | Kelembaban udara %RH |
| soil | float4 | Kelembaban tanah % |
| ph | float4 | Nilai pH tanah |
| fuzzy_suhu | float4 | Derajat keanggotaan suhu |
| fuzzy_soil | float4 | Derajat keanggotaan tanah |
| fuzzy_ph | float4 | Derajat keanggotaan pH |
| relay_kipas | float4 | Status kipas (0.0/1.0) |
| relay_air | float4 | Status pompa air (0.0/1.0) |
| relay_ph | float4 | Status pompa pH (0.0/1.0) |
| updated_at | timestamptz | Waktu insert (WIB) |

---

## 3. Interval Pengiriman

```cpp
const unsigned long INTERVAL_DB = 60000UL;  // 60 detik
unsigned long lastDBMillis = 0;
```

**`INTERVAL_DB` (60 detik)**
Data dikirim ke Supabase setiap 1 menit. Lebih jarang dibanding
MQTT (15 detik) untuk menghemat kuota API dan kapasitas database.

**`lastDBMillis`**
Menyimpan timestamp terakhir kali data dikirim ke Supabase,
digunakan sebagai referensi timer di loop utama.

---

## 4. Fungsi Insert ke Supabase

```cpp
void insertSupabase(const FuzzyOutput& fo) {
```

Fungsi menerima hasil inferensi fuzzy (`fo`) dan menggabungkannya
dengan data sensor global untuk dikirim ke database.

---

### 4a. Membangun JSON Body

```cpp
StaticJsonDocument<448> doc;
doc["temperature"] = g_suhu;
doc["humidity"]    = g_kelembaban;
doc["soil"]        = g_soil;
doc["ph"]          = g_pH;
doc["fuzzy_suhu"]  = fo.mu_kipas;
doc["fuzzy_soil"]  = fo.mu_pompa_air;
doc["fuzzy_ph"]    = fo.mu_pompa_ph;
doc["relay_kipas"] = relay_kipas_state     ? 1.0f : 0.0f;
doc["relay_air"]   = relay_pompa_air_state ? 1.0f : 0.0f;
doc["relay_ph"]    = relay_pompa_ph_state  ? 1.0f : 0.0f;
String iso = getWaktuISO();
if (iso.length() > 0) doc["updated_at"] = iso;

char body[448];
serializeJson(doc, body);
```

**`StaticJsonDocument<448>`**
Alokasi memori JSON di stack sebesar 448 byte sesuai ukuran payload.
`Static` dipilih agar tidak menggunakan heap dan menghindari
fragmentasi memori.

**`relay_*` bertipe `float4`**
Status relay dikirim sebagai `1.0f` (ON) atau `0.0f` (OFF) sesuai
tipe kolom `float4` di tabel Supabase. Tidak menggunakan boolean
karena tipe kolom di database adalah float.

**`doc["updated_at"] = iso`**
Timestamp WIB dalam format ISO 8601 (`2026-06-25T13:25:32+07:00`)
dikirim dari ESP32 untuk menggantikan nilai default `now()` Supabase
yang berbasis UTC, sehingga kolom `updated_at` menampilkan waktu
lokal WIB yang sesuai.

**`serializeJson(doc, body)`**
Mengubah objek JSON menjadi string karakter siap dikirim melalui HTTP.

---

### 4b. Membuat Koneksi TLS Lokal

```cpp
WiFiClientSecure sbClient;
sbClient.setInsecure();
```

**`WiFiClientSecure sbClient`**
Objek TLS yang dibuat **lokal** di dalam fungsi, bukan menggunakan
`tlsClient` global yang sudah dipakai oleh koneksi MQTT. Ini penting
karena jika dipakai bersama, koneksi akan bentrok dan menghasilkan
error `-5` (connection refused).

**`sbClient.setInsecure()`**
Menonaktifkan verifikasi sertifikat CA agar ESP32 bisa terhubung
ke server Supabase tanpa menyimpan sertifikat root.

---

### 4c. Memulai Koneksi HTTP

```cpp
HTTPClient http;
String url = String(SUPABASE_URL) + "/rest/v1/" + TABLE_NAME;

if (!http.begin(sbClient, url)) {
  Serial.println("[Supabase] http.begin() gagal");
  return;
}
```

**`"/rest/v1/" + TABLE_NAME`**
Endpoint REST API Supabase untuk tabel `pertanian`. Supabase secara
otomatis menghasilkan REST API dari setiap tabel PostgreSQL dengan
format URL ini.

**`http.begin(sbClient, url)`**
Menginisialisasi koneksi HTTP ke endpoint Supabase menggunakan
objek TLS lokal. Jika gagal, fungsi langsung keluar.

---

### 4d. Menambahkan Header HTTP

```cpp
http.setTimeout(10000);
http.addHeader("Content-Type",  "application/json");
http.addHeader("apikey",        SUPABASE_KEY);
http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
http.addHeader("Prefer",        "return=minimal");
```

**`http.setTimeout(10000)`**
Batas waktu 10 detik untuk menunggu respons dari server Supabase.
Mencegah fungsi memblokir loop utama terlalu lama jika server lambat.

**`Content-Type: application/json`**
Memberitahu Supabase bahwa body request berformat JSON.

**`apikey`**
Header autentikasi pertama yang wajib ada untuk setiap request ke
Supabase REST API.

**`Authorization: Bearer`**
Header autentikasi kedua berformat Bearer token, standar OAuth 2.0
yang digunakan Supabase untuk memverifikasi izin akses ke tabel.

**`Prefer: return=minimal`**
Instruksi ke Supabase agar tidak mengembalikan data baris yang baru
diinsert dalam respons. Menghemat bandwidth karena ESP32 tidak perlu
memproses data balik.

---

### 4e. Mengirim Data dan Menutup Koneksi

```cpp
int code = http.POST(body);
if (code == 201) {
    // Insert OK
} else {
    Serial.println("[Supabase] Error " + String(code)
      + ": " + http.getString());
}
http.end();
sbClient.stop();
```

**`http.POST(body)`**
Mengirimkan HTTP POST request dengan body JSON ke endpoint Supabase.
POST digunakan untuk operasi INSERT — menambahkan baris baru ke tabel
setiap kali dipanggil sehingga riwayat data tersimpan lengkap.

**`code == 201`**
HTTP status 201 Created adalah respons sukses dari Supabase yang
menandakan baris baru berhasil diinsert ke database.

**`http.getString()`**
Mengambil pesan error dari respons Supabase jika insert gagal,
berguna untuk debugging (contoh: kolom tidak cocok, key salah,
atau koneksi timeout).

**`http.end()`**
Menutup koneksi HTTP dan membebaskan resource yang digunakan
oleh objek HTTPClient.

**`sbClient.stop()`**
Menutup koneksi TLS secara eksplisit agar socket TCP dilepas
dengan bersih. Tanpa ini, koneksi lama bisa menumpuk dan
menyebabkan kehabisan socket pada ESP32.

---

## 5. Pemanggilan di Loop Utama

```cpp
if (now - lastDBMillis >= INTERVAL_DB) {
    lastDBMillis = now;
    insertSupabase(fo);
}
```

Kondisi timer — fungsi `insertSupabase()` hanya dipanggil setiap
**60 detik** (INTERVAL_DB), berbeda dengan MQTT yang dipanggil
setiap 15 detik. Pemisahan ini memungkinkan dashboard mendapat
update lebih sering via MQTT, sementara database tidak penuh terlalu
cepat.

---

## 6. Perbedaan Interval MQTT vs Supabase

| | MQTT | Supabase |
|--|------|----------|
| Interval | 15 detik | 60 detik |
| Tujuan | Real-time dashboard | Penyimpanan riwayat |
| Protokol | MQTT over TLS (port 8883) | HTTPS REST API (port 443) |
| Baris baru | Tidak (retained message) | Ya, setiap insert |
| Data 1 jam | 240 publish | 60 baris di database |

---

*File ini menjelaskan implementasi pengiriman data ke database Supabase
pada sistem monitoring greenhouse cabai rawit berbasis ESP32.*
