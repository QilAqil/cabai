
// ESP32 + Soil Moisture Sensor (analog) + DHT11
// Baca kelembaban tanah, suhu, dan kelembaban udara, lalu tampilkan ke Serial Monitor

#include <DHT.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Konfigurasi DHT11
const int DHTPIN  = 4;       // Pin data DHT11 (ubah sesuai wiring Anda, misal GPIO4)
const int DHTTYPE = DHT11;
DHT dht(DHTPIN, DHTTYPE);

// ===== WiFi =====
const char* WIFI_SSID = "UPT-LAB-KOM";
const char* WIFI_PASS = "uptlab12";

// ===== MQTT (TLS) =====
const char* mqtt_server = "n01d3130.ala.asia-southeast1.emqxsl.com";
const int   mqtt_port   = 8883;
const char* mqtt_user   = "pertanian";
const char* mqtt_pass   = "pertanian12";
const char* topic_pub   = "pertanian/sensor";
const char* topic_sub   = "pertanian/control";

WiFiClientSecure tlsClient;
PubSubClient mqttClient(tlsClient);

// ===== SUPABASE =====
#define supabaseUrl "https://sptomqebtvclfebaktof.supabase.co"
#define supabaseKey "sb_publishable_jqEF4hY0nK0Bu0BkKK2ayQ_eW_8fh_u"
#define tableName   "pertanian"

// Interval kirim data
const unsigned long MQTT_PUB_INTERVAL_MS = 10UL * 1000UL;
const unsigned long SUPABASE_INTERVAL_MS = 30UL * 1000UL;
unsigned long lastMqttPubMs = 0;
unsigned long lastSupabaseMs = 0;

// Mode kontrol relay
bool manualMode = false;
bool manualWaterOn = false;
bool manualDolomitOn = false;

// Konfigurasi soil moisture
const int SOIL_PIN   = 35;   // Pin ADC ESP32 (misal GPIO34)
const int DRY_VALUE  = 3000; // Nilai ADC saat tanah sangat kering (kalibrasi)
const int WET_VALUE  = 1000; // Nilai ADC saat tanah sangat basah (kalibrasi)

// Opsional: LED indikator (misalnya LED built-in atau LED eksternal di GPIO2)
const int LED_PIN    = 2;
const int DRY_THRESHOLD_PERCENT = 40; // Di bawah 40% dianggap kering

// Relay output
// Catatan: kebanyakan modul relay aktif-LOW (LOW = ON). Ubah jika modul Anda aktif-HIGH.
const bool RELAY_ACTIVE_LOW = true;
const int RELAY_WATER_PIN   = 26; // relay pompa air
const int RELAY_DOLOMIT_PIN = 27; // relay dolomit

// Konfigurasi sensor pH tanah (berdasarkan ph.ino)
const int DMSpin       = 13; // pin output untuk DMS (driver sensor pH)
const int PH_ADC_PIN   = 34; // pin input sensor pH tanah

int   PH_ADC;          // nilai ADC mentah untuk pH
float lastReading_pH;  // pH terakhir yang terbaca
float pH_value;        // nilai pH saat ini

// ===== Fuzzy Tahani 3x3 =====
// Input 1: kelembaban tanah (%)
//   - Kering, Lembab, Basah
// Input 2: pH tanah
//   - Asam, Netral, Basa
//
// Output (Sugeno singleton, 0..1) lalu di-threshold jadi ON/OFF relay.
//   - Water (pompa air)
//   - Dolomit

static float trimf(float x, float a, float b, float c) {
  // Handle "shoulder" degeneracies safely (a==b or b==c)
  if ((a == b) && (x == a)) return 1.0f;
  if ((b == c) && (x == c)) return 1.0f;
  if (x <= a || x >= c) return 0.0f;
  if (x == b) return 1.0f;
  if (x < b) return (x - a) / (b - a);
  return (c - x) / (c - b);
}

static float trapmf(float x, float a, float b, float c, float d) {
  // Handle degeneracies safely (a==b and/or c==d) so endpoints can still be 1.0
  if ((a == b) && (x == a)) return 1.0f;
  if ((c == d) && (x == d)) return 1.0f;
  if (x <= a || x >= d) return 0.0f;
  if (x >= b && x <= c) return 1.0f;
  if (x > a && x < b) return (x - a) / (b - a);
  return (d - x) / (d - c);
}

static float fuzzyTahaniSugeno33(const float muA[3], const float muB[3], const float out33[3][3]) {
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
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(pin, on ? LOW : HIGH);
  } else {
    digitalWrite(pin, on ? HIGH : LOW);
  }
}

static void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi connecting");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < 20000UL) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connect timeout.");
  }
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  (void)topic;
  // payload -> String sederhana agar tidak perlu library JSON
  String msg;
  msg.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();
  msg.toUpperCase();

  // Format dukungan (contoh):
  // "AUTO"
  // "MANUAL"
  // "WATER_ON" / "WATER_OFF"
  // "DOLOMIT_ON" / "DOLOMIT_OFF"
  if (msg.indexOf("AUTO") >= 0) manualMode = false;
  if (msg.indexOf("MANUAL") >= 0) manualMode = true;

  if (msg.indexOf("WATER_ON") >= 0) manualWaterOn = true;
  if (msg.indexOf("WATER_OFF") >= 0) manualWaterOn = false;

  if (msg.indexOf("DOLOMIT_ON") >= 0) manualDolomitOn = true;
  if (msg.indexOf("DOLOMIT_OFF") >= 0) manualDolomitOn = false;

  Serial.print("MQTT control: ");
  Serial.println(msg);
}

static void ensureMqtt() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqttClient.connected()) return;

  // TLS tanpa verifikasi sertifikat (paling mudah untuk mulai).
  // Jika Anda punya CA cert EMQX, nanti bisa saya pasangkan agar secure penuh.
  tlsClient.setInsecure();

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  String clientId = "esp32-pertanian-";
  clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

  Serial.print("MQTT connecting...");
  if (mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
    Serial.println("connected.");
    mqttClient.subscribe(topic_sub);
  } else {
    Serial.print("failed, rc=");
    Serial.println(mqttClient.state());
  }
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
  // Gunakan begin(url) standar (seperti banyak contoh ESP32 + HTTPS),
  // core ESP32 biasanya sudah mengatur WiFiClientSecure dengan setInsecure() di dalam.
  http.begin(url);
  http.addHeader("apikey", supabaseKey);
  http.addHeader("Authorization", String("Bearer ") + supabaseKey);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");

  // JSON row (pastikan kolom di Supabase table sesuai key di bawah)
  String body = "{";
  body += "\"temperature\":" + String(temperature, 2) + ",";
  body += "\"humidity\":" + String(humidity, 2) + ",";
  body += "\"soil\":" + String(soil) + ",";
  body += "\"ph\":" + String(ph, 2) + ",";
  body += "\"fuzzy_air\":" + String(fuzzy_air, 2) + ",";
  body += "\"fuzzy_ph\":" + String(fuzzy_ph, 2) + ",";
  // Supabase error 22P02: kolom bertipe real tidak bisa terima boolean.
  // Kirim sebagai 0/1 numerik agar kompatibel dengan tipe real/int.
  body += "\"relay_air\":" + String(relay_air ? 1 : 0) + ",";
  body += "\"relay_dolomit\":" + String(relay_dolomit ? 1 : 0);
  body += "}";

  int code = http.POST(body);
  Serial.print("Supabase HTTP code: ");
  Serial.println(code);

  if (code < 0) {
    // Error dari layer HTTPClient (misal TLS, koneksi, dll)
    Serial.print("Supabase error: ");
    Serial.println(http.errorToString(code));
  } else if (code >= 300) {
    // Print sedikit isi response untuk debugging (misal pesan error Supabase)
    String resp = http.getString();
    Serial.print("Supabase resp: ");
    Serial.println(resp);
  }

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

  Serial.println("Mulai baca sensor soil moisture dan DHT11...");
  Serial.println("Kalibrasi DRY_VALUE & WET_VALUE sesuai hasil pembacaan Anda.");
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
  // Membership kelembaban tanah (%)
  float muMoist[3];
  muMoist[0] = trapmf((float)moisturePercent, 0, 0, 25, 45);      // Kering
  muMoist[1] = trimf((float)moisturePercent, 30, 55, 80);         // Lembab
  muMoist[2] = trapmf((float)moisturePercent, 65, 85, 100, 100);  // Basah

  // Membership pH tanah
  float ph = lastReading_pH;
  float muPH[3] = {0, 0, 0};
  bool phValid = (ph >= 3.0f && ph <= 9.0f); // hanya gunakan pH dalam range wajar tanah
  if (phValid) {
    muPH[0] = trapmf(ph, 0.0f, 0.0f, 5.2f, 6.2f);   // Asam
    muPH[1] = trimf(ph, 5.8f, 6.8f, 7.8f);          // Netral
    muPH[2] = trapmf(ph, 7.2f, 8.0f, 14.0f, 14.0f); // Basa
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

  bool waterOn = manualMode ? manualWaterOn : waterOnAuto;
  bool dolomitOn = manualMode ? manualDolomitOn : dolomitOnAuto;

  relayWrite(RELAY_WATER_PIN, waterOn);
  relayWrite(RELAY_DOLOMIT_PIN, dolomitOn);

  // ===== Ringkasan singkat ke Serial Monitor (1 baris per loop) =====
  Serial.print("T=");
  Serial.print(t, 1);
  Serial.print("C H=");
  Serial.print(h, 0);
  Serial.print("% Soil=");
  Serial.print(moisturePercent);
  Serial.print("% pH=");
  Serial.print(lastReading_pH, 1);
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
    doc["ph"] = lastReading_pH;
    doc["fuzzy_air"] = waterScore;
    doc["fuzzy_ph"] = dolomitScore;
    doc["relay_air"] = waterOn ? 1 : 0;
    doc["relay_dolomit"] = dolomitOn ? 1 : 0;
    doc["mode"] = manualMode ? "MANUAL" : "AUTO";

    char buffer[256];
    size_t n = serializeJson(doc, buffer, sizeof(buffer));
    bool ok = mqttClient.publish(topic_pub, buffer, n);
    Serial.print("MQTT publish: ");
    Serial.println(ok ? "OK" : "FAIL (buffer/conn)");
  }

  // ===== Kirim ke Supabase =====
  if (now - lastSupabaseMs >= SUPABASE_INTERVAL_MS) {
    lastSupabaseMs = now;
    bool ok = supabaseInsert(t, h, moisturePercent, lastReading_pH, waterScore, dolomitScore, waterOn, dolomitOn);
    Serial.print("Supabase insert: ");
    Serial.println(ok ? "OK" : "FAIL");
  }

  waitWithMqtt(3UL * 1000UL); // jeda sebelum pembacaan berikutnya (tetap jaga MQTT)
}