/*
 * Sistem Monitoring & Kontrol Pertanian IoT Berbasis ESP32
 *
 * Fitur utama:
 * - Membaca:
 *   - Suhu & kelembaban udara (DHT11)
 *   - Kelembaban tanah (soil moisture analog)
 *   - pH tanah (sensor pH dengan driver DMS)
 * - Logika Fuzzy Tahani 3x3:
 *   - Input 1  : kelembaban tanah (Kering, Lembab, Basah)
 *   - Input 2  : pH tanah (Asam, Netral, Basa)
 *   - Output   : dua skor fuzzy (waterScore & dolomitScore)
 *   - Keputusan: mengendalikan relay pompa air & relay dolomit (otomatis via MQTT)
 * - Koneksi:
 *   - WiFi STA ke SSID laboratorium
 *   - MQTT TLS (EMQX) untuk publish data sensor ke dashboard web
 *   - Supabase REST API untuk menyimpan log data ke database (tabel 'pertanian')
 *
 * Catatan penting:
 * - Comment di file ini fokus ke penjelasan konsep, arsitektur, dan alasan pemilihan nilai,
 *   bukan sekadar menjelaskan operasi kode yang sudah jelas (misalnya "digitalWrite HIGH/LOW").
 * - Banyak parameter (batas membership fuzzy, ambang pH, mapping soil) bisa dikalibrasi ulang
 *   sesuai data lapangan, namun disini diset berdasarkan literatur & contoh di dokumen fuzzy.
 */

#include <DHT.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ==========================
// Konfigurasi Sensor DHT11
// ==========================
// DHT11 digunakan untuk mendapatkan suhu & kelembaban udara lingkungan sekitar.
// Hasilnya dipakai untuk monitoring dan dikirim ke MQTT & Supabase,
// tetapi TIDAK ikut masuk ke logika fuzzy Tahani (fuzzy fokus ke tanah & pH).
const int DHTPIN  = 4;       // Pin data DHT11 (ubah sesuai wiring Anda, misal GPIO4)
const int DHTTYPE = DHT11;
DHT dht(DHTPIN, DHTTYPE);

// ===========
// Konfigurasi WiFi
// ===========
// Mode yang dipakai: station (WIFI_STA), terkoneksi ke AP laboratorium.
// SSID & password hard-coded karena lingkungan sudah tetap (bukan user-facing product).
const char* WIFI_SSID = "UPT-LAB-KOM";
const char* WIFI_PASS = "uptlab12";
// Variabel status WiFi untuk dikirim ke dashboard (via MQTT)
bool wifiConnectedFlag = false;
String wifiIp = "";

// =====================
// Konfigurasi MQTT (TLS)
// =====================
// Menggunakan EMQX Cloud (port 8883 / TLS).
// - topic_pub : dipakai ESP32 untuk mengirim JSON data sensor ke dashboard (index.html).
const char* mqtt_server = "n01d3130.ala.asia-southeast1.emqxsl.com";
const int   mqtt_port   = 8883;
const char* mqtt_user   = "pertanian";
const char* mqtt_pass   = "pertanian12";
const char* topic_pub   = "pertanian/sensor";

WiFiClientSecure tlsClient;
PubSubClient mqttClient(tlsClient);

// ==================
// Konfigurasi Supabase
// ==================
// Menggunakan REST endpoint Supabase untuk insert baris baru ke tabel 'pertanian'.
// Kolom yang diisi: temperature, humidity, soil, ph, fuzzy_air, fuzzy_ph, relay_air, relay_dolomit.
#define supabaseUrl "https://sptomqebtvclfebaktof.supabase.co"
#define supabaseKey "sb_publishable_jqEF4hY0nK0Bu0BkKK2ayQ_eW_8fh_u"
#define tableName   "pertanian"

// Interval kirim data periodik
// - MQTT_PUB_INTERVAL_MS   : berapa sering data dipublish ke broker MQTT (dashboard real-time).
// - SUPABASE_INTERVAL_MS   : berapa sering data di-insert ke Supabase (log historis).
const unsigned long MQTT_PUB_INTERVAL_MS = 10UL * 1000UL;
const unsigned long SUPABASE_INTERVAL_MS = 30UL * 1000UL;
unsigned long lastMqttPubMs = 0;
unsigned long lastSupabaseMs = 0;

// ===========================
// Konfigurasi Sensor Soil Moisture
// ===========================
// Sensor soil moisture kapasitif analog:
// - SOIL_PIN  : input ADC ESP32 (range 0–4095).
// - DRY_VALUE : nilai ADC tipikal ketika tanah sangat kering (ditentukan dari kalibrasi).
// - WET_VALUE : nilai ADC tipikal ketika tanah sangat basah (ditentukan dari kalibrasi).
// Nilai ini kemudian dipetakan ke kelembaban tanah 0–100% secara linear.
const int SOIL_PIN   = 35;   // Pin ADC ESP32 (misal GPIO34)
const int DRY_VALUE  = 3000; // Nilai ADC saat tanah sangat kering (kalibrasi)
const int WET_VALUE  = 1000; // Nilai ADC saat tanah sangat basah (kalibrasi)

// LED indikator (biasanya LED built-in di GPIO2):
// - Dipakai sebagai indikator tanah kering dan status pembacaan pH.
const int LED_PIN    = 2;
const int DRY_THRESHOLD_PERCENT = 40; // Di bawah 40% dianggap kering

// ============
// Relay output
// ============
// Mengendalikan dua relay:
// - RELAY_WATER_PIN   : pompa air
// - RELAY_DOLOMIT_PIN : aplikasi dolomit
// Asumsi modul relay aktif-LOW (umum di pasaran): LOW = menyala, HIGH = mati.
// Jika modul yang digunakan aktif-HIGH, atur RELAY_ACTIVE_LOW = false.
const bool RELAY_ACTIVE_LOW = true;
const int RELAY_WATER_PIN   = 26; // relay pompa air
const int RELAY_DOLOMIT_PIN = 27; // relay dolomit

// =============================
// Konfigurasi Sensor pH Tanah
// =============================
// Sensor pH tanah membutuhkan driver/modul DMS:
// - DMSpin    : output untuk mengaktifkan modul pengondisi sinyal (DMS).
// - PH_ADC_PIN: input ADC yang membaca tegangan keluaran sensor pH.
// Pembacaan dilakukan secara periodik dengan delay agar DMS sempat stabil (10 detik).
const int DMSpin       = 13; // pin output untuk DMS (driver sensor pH)
const int PH_ADC_PIN   = 34; // pin input sensor pH tanah

int   PH_ADC;          // nilai ADC mentah untuk pH
float lastReading_pH;  // pH terakhir yang terbaca
float pH_value;        // nilai pH saat ini

// =========================
// Fuzzy Tahani 3x3 (Sugeno)
// =========================
//
// Kesesuaian dengan dokumen "Logika Fuzzy dan Metode Fuzzy Tahani" (fuzzy.text):
//
// 1. MEMBERSHIP FUNCTION (fuzzy.text: "inti sistem fuzzy", nilai 0–1)
//    - Kelembaban tanah: Kering (trapesium), Lembab (segitiga), Basah (trapesium).
//      Contoh dokumen: "kering 0–50%", "lembab 30–70%" → diimplementasi dengan overlap
//      trapmf(0,0,30,50), trimf(30,50,70), trapmf(60,80,100,100).
//    - pH tanah: Asam, Netral, Basa (semua trapesium). Dokumen: "pH optimal 6–6,8 netral"
//      → Netral pakai trapmf dengan plateau 6–6,8.
//    - Jenis kurva: dokumen menyebut "kurva segitiga atau trapesium lebih efisien" untuk IoT ✅
//
// 2. FUZZIFIKASI (fuzzy.text: "nilai tegas → derajat keanggotaan")
//    - Input: soil% dan pH. Output: muMoist[3], muPH[3] (derajat untuk tiap himpunan). ✅
//
// 3. FIRE STRENGTH / OPERATOR ZADEH (fuzzy.text: "Interseksi AND = min(μA, μB)")
//    - Bobot tiap rule: w_ij = min(μA_i, μB_j). Di kode: min(muA[i], muB[j]). ✅
//
// 4. REKOMENDASI KEPUTUSAN (fuzzy.text: "fire strength 0–1, alternatif nilai tertinggi")
//    - Di sini dua keluaran (penyiraman & dolomit). Agregasi pakai model Sugeno singleton:
//      crisp = Σ(w_ij * z_ij) / Σ(w_ij). Nilai crisp 0..1 lalu di-ambang 0.5 → ON/OFF relay.
//    - "Alternatif nilai tertinggi" diwujudkan sebagai: skor > 0.5 = rekomendasi jalankan. ✅
//
// 5. RINGKASAN ALUR
//    - Fuzzifikasi: soil% & pH → muMoist[3], muPH[3].
//    - Inferensi: AND = min (Tahani/Zadeh); setiap rule (i,j) punya singleton z_ij.
//    - Defuzzifikasi: weighted average (Sugeno).
//    - Keputusan: waterOn = (waterScore >= 0.5), dolomitOn = (dolomitScore >= 0.5).

static float trimf(float x, float a, float b, float c) {
  // Fungsi keanggotaan segitiga:
  // a = batas kiri (μ=0), b = puncak (μ=1), c = batas kanan (μ=0).
  // Dipakai untuk himpunan "Lembab" karena transisi halus dan perhitungan ringan.
  // Handle "shoulder" degeneracies safely (a==b atau b==c) agar tetap stabil di ujung.
  if ((a == b) && (x == a)) return 1.0f;
  if ((b == c) && (x == c)) return 1.0f;
  if (x <= a || x >= c) return 0.0f;
  if (x == b) return 1.0f;
  if (x < b) return (x - a) / (b - a);
  return (c - x) / (c - b);
}

static float trapmf(float x, float a, float b, float c, float d) {
  // Fungsi keanggotaan trapesium:
  // a = mulai naik dari 0, b = mulai plateau 1, c = akhir plateau 1, d = turun ke 0.
  // Cocok ketika ada rentang nilai yang dianggap optimal penuh (μ=1), misalnya pH netral 6–6.8.
  // Penanganan kasus a==b atau c==d dilakukan agar nilai ujung bisa tetap 1.0 tanpa NaN.
  if ((a == b) && (x == a)) return 1.0f;
  if ((c == d) && (x == d)) return 1.0f;
  if (x <= a || x >= d) return 0.0f;
  if (x >= b && x <= c) return 1.0f;
  if (x > a && x < b) return (x - a) / (b - a);
  return (d - x) / (d - c);
}

static float fuzzyTahaniSugeno33(const float muA[3], const float muB[3], const float out33[3][3]) {
  // Mengimplementasikan inferensi Tahani 3x3 dengan skema Sugeno:
  // - muA[3] : derajat keanggotaan input 1 (soil) untuk {Kering, Lembab, Basah}
  // - muB[3] : derajat keanggotaan input 2 (pH)   untuk {Asam, Netral, Basa}
  // - out33  : matriks output singleton (0..1) untuk setiap kombinasi (i,j)
  //
  // Langkah:
  // 1. Untuk setiap rule (i,j), hitung bobot w_ij = min(μA_i, μB_j)  [operator AND Zadeh].
  // 2. Akumulasi sumW  = Σ w_ij, sumWZ = Σ (w_ij * z_ij).
  // 3. Nilai crisp = sumWZ / sumW jika sumW > 0, jika tidak maka 0.
  float sumW = 0.0f;
  float sumWZ = 0.0f;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      float w = (muA[i] < muB[j]) ? muA[i] : muB[j]; // AND = min (Tahani)
      sumW += w;
      sumWZ += w * out33[i][j];
    }
  }
  return (sumW > 0.0f) ? (sumWZ / sumW) : 0.0f;
}

static void relayWrite(int pin, bool on) {
  // Abstraksi kecil untuk menghilangkan kebingungan aktif-LOW vs aktif-HIGH.
  // - Jika RELAY_ACTIVE_LOW = true:
  //     on  = true  -> tulis LOW (relay menyala)
  //     on  = false -> tulis HIGH (relay mati)
  // - Jika RELAY_ACTIVE_LOW = false (modul aktif-HIGH), kebalikannya.
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(pin, on ? LOW : HIGH);
  } else {
    digitalWrite(pin, on ? HIGH : LOW);
  }
}

static void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnectedFlag = true;
    wifiIp = WiFi.localIP().toString();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < 20000UL) {
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnectedFlag = true;
    wifiIp = WiFi.localIP().toString();
  } else {
    wifiConnectedFlag = false;
    wifiIp = "";
  }
}

static void ensureMqtt() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqttClient.connected()) return;

  // TLS tanpa verifikasi sertifikat (paling mudah untuk mulai).
  // Jika Anda punya CA cert EMQX, nanti bisa saya pasangkan agar secure penuh.
  tlsClient.setInsecure();

  mqttClient.setServer(mqtt_server, mqtt_port);

  String clientId = "esp32-pertanian-";
  clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

  mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_pass);
}

static void waitWithMqtt(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    mqttClient.loop();
    delay(50);
  }
}

static bool supabaseInsert(
  float temperature, float humidity, int soil,
  float ph, float fuzzy_air, float fuzzy_ph,
  bool relay_air, bool relay_dolomit
) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  String url = String(supabaseUrl) + "/rest/v1/" + tableName;
  // Endpoint Supabase:
  // - Metode    : POST
  // - URL       : {supabaseUrl}/rest/v1/{tableName}
  // - Header    : apikey, Authorization, Content-Type, Prefer
  // - Body JSON : satu objek mewakili satu baris data sensor.
  http.begin(url);
  http.addHeader("apikey", supabaseKey);
  http.addHeader("Authorization", String("Bearer ") + supabaseKey);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");

  // JSON row (pastikan kolom di Supabase table sesuai key di bawah)
  // Tabel 'pertanian' diasumsikan memiliki kolom:
  // id (serial), updated_at (timestamp, default now()), temperature, humidity,
  // soil, ph, fuzzy_air, fuzzy_ph, relay_air, relay_dolomit.
  String body = "{";
  body += "\"temperature\":" + String(temperature, 2) + ",";
  body += "\"humidity\":" + String(humidity, 2) + ",";
  body += "\"soil\":" + String(soil) + ",";
  // pH dibulatkan 1 angka di belakang koma
  body += "\"ph\":" + String(ph, 1) + ",";
  body += "\"fuzzy_air\":" + String(fuzzy_air, 2) + ",";
  body += "\"fuzzy_ph\":" + String(fuzzy_ph, 2) + ",";
  // Supabase error 22P02: kolom bertipe real tidak bisa terima boolean.
  // Kirim sebagai 0/1 numerik agar kompatibel dengan tipe real/int.
  body += "\"relay_air\":" + String(relay_air ? 1 : 0) + ",";
  body += "\"relay_dolomit\":" + String(relay_dolomit ? 1 : 0);
  body += "}";

  int code = http.POST(body);

  http.end();
  return (code >= 200 && code < 300);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // PubSubClient default buffer sering terlalu kecil untuk JSON.
  // Samakan pendekatan dengan fuzzy.ino (buffer cukup besar).
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(60);

  pinMode(SOIL_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

   // Sensor pH tanah
  pinMode(DMSpin, OUTPUT);
  digitalWrite(DMSpin, HIGH); // non-aktifkan DMS di awal

  // Relay
  pinMode(RELAY_WATER_PIN, OUTPUT);
  pinMode(RELAY_DOLOMIT_PIN, OUTPUT);
  relayWrite(RELAY_WATER_PIN, false);
  relayWrite(RELAY_DOLOMIT_PIN, false);

  dht.begin();

  ensureWiFi();
  ensureMqtt();

  Serial.println("Mulai monitoring pertanian IoT...");
}

void loop() {
  ensureWiFi();
  ensureMqtt();
  mqttClient.loop();

  // Baca nilai analog (0 - 4095 untuk ESP32)
  int sensorValue = analogRead(SOIL_PIN);

  // Konversi ke persen kelembaban (0–100%)
  // Catatan: pada banyak sensor, nilai ADC KECIL = tanah BASAH, BESAR = tanah KERING
  int moisturePercent = map(sensorValue, DRY_VALUE, WET_VALUE, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);

  // Baca DHT11
  float h = dht.readHumidity();
  float t = dht.readTemperature(); // default Celcius

  // Jika DHT gagal, jangan spam banyak teks; cukup satu baris singkat.
  if (isnan(h) || isnan(t)) {
    Serial.println("DHT error");
    h = 0;
    t = 0;
  }

  // Nyalakan LED kalau tanah kering
  if (moisturePercent < DRY_THRESHOLD_PERCENT) {
    digitalWrite(LED_PIN, HIGH);  // Tanah kering -> LED ON (butuh disiram)
  } else {
    digitalWrite(LED_PIN, LOW);   // Tanah cukup lembab
  }

  // ===== Pembacaan sensor pH tanah (berdasarkan ph.ino) =====
  // Aktifkan DMS dan LED indikator
  digitalWrite(DMSpin, LOW);      // aktifkan DMS
  digitalWrite(LED_PIN, HIGH);    // LED indikator menyala saat pembacaan pH
  waitWithMqtt(10UL * 1000UL);    // tunggu DMS capture data (10 detik) sambil jaga MQTT

  PH_ADC = analogRead(PH_ADC_PIN); 

  // Konversi ADC (12-bit) ke skala 10-bit seperti di ph.ino, lalu hitung pH
  float adc10bit = PH_ADC / 4.0;  // 0–4095 -> ~0–1023
  float pH_raw = (-0.0233 * adc10bit) + 12.698;  // rumus regresi linier konversi adc ke pH

  // Deteksi koneksi sensor pH:
  // - Soil pH normal kira-kira di 3.0–9.0
  // - Jika di luar range, besar kemungkinan sensor tidak terhubung / pembacaan tidak valid
  bool phInSoilRange = (pH_raw >= 3.0f && pH_raw <= 9.0f);

  if (phInSoilRange) {
    pH_value = pH_raw;
    lastReading_pH = pH_value;
  } else {
    // Anggap sensor tidak terhubung / tidak valid
    pH_value = 0.0f;
    lastReading_pH = 0.0f;
  }

  // Nonaktifkan DMS dan matikan LED indikator setelah pembacaan pH
  digitalWrite(DMSpin, HIGH);
  digitalWrite(LED_PIN, LOW);

  // ===== Fuzzy Tahani 3x3 untuk relay =====
  // Membership kelembaban tanah (%) mengikuti contoh pada fuzzy.text:
  // - Kering   : tinggi pada 0–30%, turun ke 0 di 50%
  // - Lembab   : segitiga, puncak di 50%, nol di 30% dan 70%
  // - Basah    : mulai naik sekitar 60%, penuh di 80–100%
  float muMoist[3];
  muMoist[0] = trapmf((float)moisturePercent, 0.0f, 0.0f, 30.0f, 50.0f);   // Kering
  muMoist[1] = trimf((float)moisturePercent, 30.0f, 50.0f, 70.0f);         // Lembab
  muMoist[2] = trapmf((float)moisturePercent, 60.0f, 80.0f, 100.0f, 100.0f); // Basah

  // Membership pH tanah mengikuti penjelasan fuzzy.text:
  // - Asam   : kuat di bawah ~5, turun hingga 6
  // - Netral : trapesium dengan plateau pH optimal 6–6.8
  // - Basa   : naik mulai ~6.8 sampai 9
  float ph = lastReading_pH;
  float muPH[3] = {0, 0, 0};
  bool phValid = (ph >= 3.0f && ph <= 9.0f); // hanya gunakan pH dalam range wajar tanah
  if (phValid) {
    muPH[0] = trapmf(ph, 3.0f, 3.0f, 5.0f, 6.0f);            // Asam
    muPH[1] = trapmf(ph, 5.5f, 6.0f, 6.8f, 7.2f);            // Netral (plateau 6–6.8)
    muPH[2] = trapmf(ph, 6.8f, 8.0f, 9.0f, 9.0f);            // Basa
  }

  // Rule base 3x3 (baris = moisture: Kering/Lembab/Basah, kolom = pH: Asam/Netral/Basa)
  // Output singleton 0..1
  // Water: dominan dipicu tanah kering.
  const float WATER_OUT[3][3] = {
    {1.0f, 1.0f, 1.0f},  // Kering
    {0.4f, 0.2f, 0.3f},  // Lembab
    {0.0f, 0.0f, 0.0f}   // Basah
  };

  // Dolomit: dominan dipicu pH asam, dikurangi jika tanah terlalu basah.
  const float DOLOMIT_OUT[3][3] = {
    {0.6f, 0.0f, 0.0f},  // Kering
    {1.0f, 0.1f, 0.0f},  // Lembab
    {0.3f, 0.0f, 0.0f}   // Basah
  };

  float waterScore = 0.0f;
  float dolomitScore = 0.0f;
  if (phValid) {
    waterScore = fuzzyTahaniSugeno33(muMoist, muPH, WATER_OUT);
    dolomitScore = fuzzyTahaniSugeno33(muMoist, muPH, DOLOMIT_OUT);
  }

  bool waterOnAuto = phValid && (waterScore >= 0.5f);
  bool dolomitOnAuto = phValid && (dolomitScore >= 0.5f);

  // Keputusan akhir relay: hanya berdasarkan fuzzy (otomatis penuh, tanpa override manual).
  bool waterOn = waterOnAuto;
  bool dolomitOn = dolomitOnAuto;

  relayWrite(RELAY_WATER_PIN, waterOn);
  relayWrite(RELAY_DOLOMIT_PIN, dolomitOn);

  // ===== Ringkasan singkat ke Serial Monitor (1 baris per loop) =====
  float phRounded = roundf(lastReading_pH * 10.0f) / 10.0f;
  Serial.print("T=");
  Serial.print(t, 1);
  Serial.print("C H=");
  Serial.print(h, 0);
  Serial.print("% Soil=");
  Serial.print(moisturePercent);
  Serial.print("% pH=");
  Serial.print(phRounded, 1);
  Serial.print(" W=");
  Serial.print(waterOn ? 1 : 0);
  Serial.print(" D=");
  Serial.print(dolomitOn ? 1 : 0);
  Serial.println();

  // ===== Publish MQTT =====
  unsigned long now = millis();
  if (mqttClient.connected() && (now - lastMqttPubMs >= MQTT_PUB_INTERVAL_MS)) {
    lastMqttPubMs = now;

    // JSON payload (sesuai fuzzy.ino: ArduinoJson + char buffer)
    StaticJsonDocument<256> doc;
    doc["temperature"] = t;
    doc["humidity"] = h;
    doc["soil"] = moisturePercent;
    // Kirim pH dengan 1 angka di belakang koma
    doc["ph"] = phRounded;
    doc["fuzzy_air"] = waterScore;
    doc["fuzzy_ph"] = dolomitScore;
    doc["relay_air"] = waterOn ? 1 : 0;
    doc["relay_dolomit"] = dolomitOn ? 1 : 0;
    // Status WiFi untuk ditampilkan di dashboard
    doc["wifi_connected"] = wifiConnectedFlag ? 1 : 0;
    doc["wifi_ip"] = wifiIp;

    char buffer[256];
    size_t n = serializeJson(doc, buffer, sizeof(buffer));
    mqttClient.publish(topic_pub, buffer, n);
  }

  // ===== Kirim ke Supabase =====
  if (now - lastSupabaseMs >= SUPABASE_INTERVAL_MS) {
    lastSupabaseMs = now;
    supabaseInsert(t, h, moisturePercent, phRounded, waterScore, dolomitScore, waterOn, dolomitOn);
  }

  waitWithMqtt(3UL * 1000UL); // jeda sebelum pembacaan berikutnya (tetap jaga MQTT)
}