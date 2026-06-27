/*
 * Sistem Monitoring & Kontrol Greenhouse CABAI RAWIT — ESP32
 * Parameter optimal: Suhu 24-28°C | Kelembaban tanah 50-70% | pH 6-7
 * Metode kontrol : Fuzzy Tahani (implikasi MIN, agregasi MAX, defuzzifikasi centroid)
 * Aktuator       : Kipas GPIO25 | Pompa air GPIO26 | Koreksi pH GPIO27
 * Sensor         : DHT22 GPIO4 | Soil AO GPIO35 | pH ADC GPIO34 + DMS GPIO13
 */

#include <DHT.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ── Konfigurasi DHT22 ────────────────────────────────────────────
#define DHTPIN  4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ── Konfigurasi WiFi ─────────────────────────────────────────────
const char* WIFI_SSID = "UPT-LAB-KOM";
const char* WIFI_PASS = "uptlab12";
bool   wifiConnectedFlag = false;
String wifiIp = "";

// ── Konfigurasi MQTT (EMQX Cloud, TLS port 8883) ─────────────────
const char* MQTT_SERVER = "n01d3130.ala.asia-southeast1.emqxsl.com";
const int   MQTT_PORT   = 8883;
const char* MQTT_USER   = "pertanian";
const char* MQTT_PASS   = "pertanian12";
const char* TOPIC_PUB   = "pertanian/sensor";
WiFiClientSecure tlsClient;
PubSubClient     mqttClient(tlsClient);

// ── Konfigurasi Supabase ─────────────────────────────────────────
#define SUPABASE_URL "https://sptomqebtvclfebaktof.supabase.co"
#define SUPABASE_KEY "sb_publishable_jqEF4hY0nK0Bu0BkKK2ayQ_eW_8fh_u"
#define TABLE_NAME   "pertanian"

// ── Interval pengiriman data ─────────────────────────────────────
const unsigned long MQTT_INTERVAL_MS     = 10000UL; // publish MQTT tiap 10 detik
const unsigned long SUPABASE_INTERVAL_MS = 60000UL; // insert Supabase tiap 60 detik
unsigned long lastMqttMs = 0, lastSupabaseMs = 0;

// ── Konfigurasi sensor soil moisture (GPIO35) ────────────────────
const int  SOIL_PIN      = 35;
const int  SOIL_PWR_PIN  = 14;   // GPIO14 = saklar daya sensor (power gating)
const int  DRY_VALUE     = 3200; // ADC saat sensor di udara kering → 0%
const int  WET_VALUE     = 150;  // ADC saat sensor di tanah sangat basah → 100%
const int  SOIL_SAMPLES  = 10;   // jumlah sampel median per pembacaan

// ── Konfigurasi sensor pH (GPIO34 + DMS GPIO13) ──────────────────
const int  PH_ADC_PIN    = 34;
const int  DMS_PIN       = 13;
const int  LED_PIN       = 2;    // LED indikator saat baca pH
const int  PH_SAMPLES    = 15;   // jumlah sampel median pH
const int  PH_ADC_MIN    = 200;  // batas bawah ADC valid
const int  PH_ADC_MAX    = 3800; // batas atas ADC valid
const int  PH_SPREAD_MAX = 60;   // toleransi noise/spread ADC pH

// Koreksi bias galvanik saat soil dan pH dalam 1 wadah:
// Nilai = pH_terbaca - pH_referensi_manual. Set 0.0 jika wadah terpisah.
const float PH_BIAS      = 1.9f;
const int   SOIL_MIN_FOR_BIAS = 35; // koreksi hanya aktif jika tanah >= 35%

// Jeda waktu antar fase baca sensor (menghindari interferensi galvanik)
const unsigned long SOIL_OFF_BEFORE_PH_MS = 3000UL; // soil OFF → tunggu → baca pH
const unsigned long SOIL_ON_AFTER_PH_MS   = 2500UL; // pH selesai → tunggu → soil ON

// ── Konfigurasi relay aktuator (aktif-LOW) ───────────────────────
const bool RELAY_ACTIVE_LOW  = true;
const int  RELAY_KIPAS_PIN   = 25;
const int  RELAY_AIR_PIN     = 26;
const int  RELAY_PH_PIN      = 27;
const float FUZZY_THRESHOLD  = 0.5f; // aktuator ON jika skor centroid >= 0.5

// ── Status relay sebelumnya (hanya tulis saat ada perubahan) ─────
bool prevKipas = false, prevAir = false, prevPh = false;

// ════════════════════════════════════════════════════════════════
// FUNGSI UTILITAS
// ════════════════════════════════════════════════════════════════

// adcFlushPin: buang n sampel ADC awal agar kapasitor charge stabil
static void adcFlushPin(int pin, int n) {
  for (int i = 0; i < n; i++) { analogRead(pin); delay(5); }
}

// bacaAdcMedian: ambil median dari nSamples bacaan ADC, hitung spread
static int bacaAdcMedian(int pin, int nSamples, int &spread) {
  if (nSamples > 30) nSamples = 30;
  int v[30];
  for (int i = 0; i < nSamples; i++) { v[i] = analogRead(pin); delay(5); }
  // Bubble sort ascending
  for (int i = 0; i < nSamples - 1; i++)
    for (int j = 0; j < nSamples - i - 1; j++)
      if (v[j] > v[j+1]) { int tmp = v[j]; v[j] = v[j+1]; v[j+1] = tmp; }
  // Spread dari nilai ke-2 s/d ke-(n-2) — abaikan ekstrem terluar
  spread = (nSamples >= 5) ? v[nSamples-2] - v[1] : v[nSamples-1] - v[0];
  return v[nSamples / 2]; // median
}

// soilPower: nyalakan/matikan daya sensor tanah via GPIO14
static void soilPower(bool on) {
  if (SOIL_PWR_PIN >= 0) digitalWrite(SOIL_PWR_PIN, on ? HIGH : LOW);
}

// relayWrite: tulis relay dengan logika aktif-LOW atau aktif-HIGH
static void relayWrite(int pin, bool on) {
  digitalWrite(pin, (RELAY_ACTIVE_LOW ? !on : on) ? HIGH : LOW);
}

// relayUpdate: tulis relay HANYA jika status berubah (kurangi bouncing)
static void relayUpdate(int pin, bool on, bool &prev) {
  if (on != prev) { relayWrite(pin, on); prev = on; }
}

// waitMqtt: tunggu ms milidetik sambil tetap jaga koneksi MQTT aktif
static void waitMqtt(unsigned long ms) {
  unsigned long t = millis();
  while (millis() - t < ms) { mqttClient.loop(); delay(50); }
}

// adcKePh: konversi nilai ADC 12-bit ke nilai pH (kalibrasi: ADC=845 → pH=7.0)
static float adcKePh(int adc) {
  return (-0.0233f * (adc / 4.0f)) + 11.922f;
}

// getTimeStr: format waktu dari NTP, fallback ke uptime jika belum sync
String getTimeStr() {
  struct tm ti;
  if (!getLocalTime(&ti, 10)) {
    unsigned long s = millis() / 1000;
    char buf[16];
    sprintf(buf, "[%02lu:%02lu:%02lu]", s/3600, (s/60)%60, s%60);
    return String(buf);
  }
  char buf[32];
  strftime(buf, sizeof(buf), "[%H:%M:%S]", &ti);
  return String(buf);
}

// ════════════════════════════════════════════════════════════════
// FUZZY TAHANI — fungsi keanggotaan & defuzzifikasi
// ════════════════════════════════════════════════════════════════

static float fmin2(float a, float b) { return a < b ? a : b; }
static float fmax2(float a, float b) { return a > b ? a : b; }

// trimf: fungsi keanggotaan segitiga
static float trimf(float x, float a, float b, float c) {
  if (x <= a || x >= c) return 0.0f;
  return (x < b) ? (x-a)/(b-a) : (c-x)/(c-b);
}

// trapmf: fungsi keanggotaan trapesium
static float trapmf(float x, float a, float b, float c, float d) {
  if (x <= a || x >= d) return 0.0f;
  if (x >= b && x <= c) return 1.0f;
  return (x < b) ? (x-a)/(b-a) : (d-x)/(d-c);
}

// fuzzyTahani: defuzzifikasi centroid dengan implikasi MIN dan agregasi MAX
// Output domain [0,1] dengan 3 himpunan: Rendah/Sedang/Tinggi
static float fuzzyTahani(float muR, float muS, float muT) {
  // Fungsi keanggotaan output (precomputed inline, 21 titik diskrit)
  static const float Y[21] = {
    0.00f,0.05f,0.10f,0.15f,0.20f,0.25f,0.30f,0.35f,0.40f,0.45f,0.50f,
    0.55f,0.60f,0.65f,0.70f,0.75f,0.80f,0.85f,0.90f,0.95f,1.00f
  };
  float num = 0.0f, den = 0.0f;
  for (int i = 0; i <= 20; i++) {
    float y   = Y[i];
    float outR = trapmf(y, 0.0f, 0.0f, 0.15f, 0.45f); // output Rendah
    float outS = trimf (y, 0.15f, 0.35f, 0.55f);       // output Sedang
    float outT = trapmf(y, 0.45f, 0.65f, 1.0f, 1.0f);  // output Tinggi
    // Implikasi MIN per rule, agregasi MAX
    float agg = fmax2(fmax2(fmin2(muR, outR), fmin2(muS, outS)), fmin2(muT, outT));
    num += y * agg;
    den += agg;
  }
  return (den > 1e-6f) ? (num / den) : 0.0f;
}

// fuzzyTemp: suhu → skor kipas
static float fuzzyTemp(float t) {
  float muR = trapmf(t, 0.0f, 0.0f, 24.0f, 27.0f); // Rendah
  float muS = trimf (t, 24.0f, 27.0f, 31.0f);       // Sedang
  float muT = trapmf(t, 27.0f, 31.0f, 45.0f, 45.0f);// Tinggi
  return fuzzyTahani(muR, muS, muT);
}

// fuzzySoil: kelembaban tanah → skor pompa air
// Kering → skor Tinggi (air perlu dinyalakan)
static float fuzzySoil(int pct) {
  float x  = (float)pct;
  float muK = trapmf(x, 0.0f, 0.0f, 40.0f, 50.0f);  // Kering → OUT_TINGGI
  float muL = trapmf(x, 40.0f, 50.0f, 70.0f, 80.0f); // Lembab → OUT_RENDAH
  float muB = trapmf(x, 70.0f, 80.0f, 100.0f, 100.0f);// Basah  → OUT_RENDAH
  // Rule: Kering→Tinggi, Lembab→Rendah, Basah→Rendah
  // Agar tanah kering → skor tinggi → pompa ON, petakan ke (muT=muK, muS=0, muR=max(muL,muB))
  return fuzzyTahani(fmax2(muL, muB), 0.0f, muK);
}

// fuzzyPh: pH → skor relay koreksi
// Asam atau Basa → skor Tinggi (koreksi perlu)
static float fuzzyPh(float ph) {
  float muA = trapmf(ph, 3.0f, 3.0f, 5.0f, 6.0f);   // Asam   → OUT_TINGGI
  float muN = trapmf(ph, 5.5f, 6.0f, 7.0f, 7.5f);   // Normal → OUT_RENDAH
  float muB = trapmf(ph, 7.0f, 7.5f, 9.0f, 9.0f);   // Basa   → OUT_TINGGI
  return fuzzyTahani(muN, 0.0f, fmax2(muA, muB));
}

// ════════════════════════════════════════════════════════════════
// KONEKSI WiFi & MQTT
// ════════════════════════════════════════════════════════════════

static void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnectedFlag = true;
    wifiIp = WiFi.localIP().toString();
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 20000UL) delay(500);
  wifiConnectedFlag = (WiFi.status() == WL_CONNECTED);
  wifiIp = wifiConnectedFlag ? WiFi.localIP().toString() : "";
}

static void ensureMqtt() {
  if (!wifiConnectedFlag || mqttClient.connected()) return;
  tlsClient.setInsecure();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  String id = "esp32-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  mqttClient.connect(id.c_str(), MQTT_USER, MQTT_PASS);
}

// ════════════════════════════════════════════════════════════════
// KIRIM DATA
// ════════════════════════════════════════════════════════════════

static void publishMqtt(float t, float h, int soil, float ph, bool phValid,
                        float fs, float fso, float fph,
                        bool kipas, bool air, bool phRelay) {
  if (!mqttClient.connected()) return;
  StaticJsonDocument<384> doc;
  doc["temperature"]   = t;
  doc["humidity"]      = h;
  doc["soil"]          = soil;
  doc["ph_valid"]      = phValid ? 1 : 0;
  doc["ph"]            = phValid ? roundf(ph * 10.0f) / 10.0f : 0.0f;
  doc["fuzzy_suhu"]    = fs;
  doc["fuzzy_soil"]    = fso;
  doc["fuzzy_ph"]      = fph;
  doc["relay_kipas"]   = kipas ? 1 : 0;
  doc["relay_air"]     = air   ? 1 : 0;
  doc["relay_ph"]      = phRelay ? 1 : 0;
  doc["wifi_connected"]= wifiConnectedFlag ? 1 : 0;
  doc["wifi_ip"]       = wifiIp;
  char buf[384];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  mqttClient.publish(TOPIC_PUB, buf, n);
}

static void insertSupabase(float t, float h, int soil, float ph, bool phValid,
                           float fs, float fso, float fph,
                           bool kipas, bool air, bool phRelay) {
  if (!wifiConnectedFlag) return;
  HTTPClient http;
  http.begin(String(SUPABASE_URL) + "/rest/v1/" + TABLE_NAME);
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", "Bearer " SUPABASE_KEY);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");
  StaticJsonDocument<384> doc;
  doc["temperature"] = t;
  doc["humidity"]    = h;
  doc["soil"]        = soil;
  doc["ph"]          = phValid ? roundf(ph * 10.0f) / 10.0f : 0.0f;
  doc["fuzzy_suhu"]  = fs;
  doc["fuzzy_soil"]  = fso;
  doc["fuzzy_ph"]    = fph;
  doc["relay_kipas"] = kipas  ? 1 : 0;
  doc["relay_air"]   = air    ? 1 : 0;
  doc["relay_ph"]    = phRelay? 1 : 0;
  char body[384];
  serializeJson(doc, body, sizeof(body));
  int code = http.POST((uint8_t*)body, strlen(body));
  if (code < 200 || code >= 300)
    Serial.printf("Supabase error %d\n", code);
  http.end();
}

// ════════════════════════════════════════════════════════════════
// SETUP
// ════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(500);

  // Inisialisasi ADC
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  analogSetPinAttenuation(SOIL_PIN, ADC_11db);
  analogSetPinAttenuation(PH_ADC_PIN, ADC_11db);

  // Inisialisasi pin
  pinMode(SOIL_PIN, INPUT);
  if (SOIL_PWR_PIN >= 0) { pinMode(SOIL_PWR_PIN, OUTPUT); soilPower(false); }
  pinMode(DMS_PIN, OUTPUT);  digitalWrite(DMS_PIN, HIGH); // DMS OFF default
  pinMode(LED_PIN, OUTPUT);  digitalWrite(LED_PIN, LOW);
  pinMode(RELAY_KIPAS_PIN, OUTPUT); relayWrite(RELAY_KIPAS_PIN, false);
  pinMode(RELAY_AIR_PIN,   OUTPUT); relayWrite(RELAY_AIR_PIN,   false);
  pinMode(RELAY_PH_PIN,    OUTPUT); relayWrite(RELAY_PH_PIN,    false);

  dht.begin();

  // Koneksi jaringan
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(60);
  ensureWiFi();
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  ensureMqtt();

  // Warm-up sensor tanah: buang 30 sampel awal agar ADC stabil
  Serial.print("Warm-up soil");
  for (int i = 0; i < 30; i++) { analogRead(SOIL_PIN); delay(50); }
  Serial.println(" OK");
}

// ════════════════════════════════════════════════════════════════
// LOOP UTAMA
// ════════════════════════════════════════════════════════════════

void loop() {
  ensureWiFi();
  ensureMqtt();
  mqttClient.loop();

  // ── 1. Baca DHT22 (suhu & kelembaban udara) ──────────────────
  static bool   dhtReady = false;
  static float  t = 26.0f, h = 60.0f;
  float tRaw = dht.readTemperature();
  float hRaw = dht.readHumidity();
  if (!isnan(tRaw) && !isnan(hRaw)) {
    t = roundf(tRaw * 10.0f) / 10.0f;
    h = roundf(hRaw * 10.0f) / 10.0f;
    dhtReady = true;
  }

  // ── 2. Baca pH (soil OFF dulu agar tidak ada interferensi) ───
  soilPower(false);
  waitMqtt(SOIL_OFF_BEFORE_PH_MS); // tunggu arus galvanik larutan habis

  digitalWrite(DMS_PIN, LOW);      // aktifkan DMS
  digitalWrite(LED_PIN, HIGH);
  waitMqtt(2000);                  // tunggu elektroda stabil
  adcFlushPin(PH_ADC_PIN, 6);      // buang sampel transien

  int  phSpread = 0;
  int  phAdc    = bacaAdcMedian(PH_ADC_PIN, PH_SAMPLES, phSpread);
  float phRaw   = adcKePh(phAdc);
  phRaw = constrain(phRaw, 3.0f, 9.0f);

  digitalWrite(DMS_PIN, HIGH);     // matikan DMS segera (cegah polarisasi)
  digitalWrite(LED_PIN, LOW);

  // Validasi pembacaan pH
  bool phOk = (phAdc >= PH_ADC_MIN) && (phAdc <= PH_ADC_MAX) &&
              (phSpread <= PH_SPREAD_MAX) && (phRaw >= 3.0f) && (phRaw <= 9.0f);

  // ── 3. Baca kelembaban tanah ─────────────────────────────────
  waitMqtt(SOIL_ON_AFTER_PH_MS);
  soilPower(true);
  adcFlushPin(SOIL_PIN, 10); // buang sampel transien setelah daya naik

  int soilSpread = 0;
  int soilAdc    = bacaAdcMedian(SOIL_PIN, SOIL_SAMPLES, soilSpread);

  // Deteksi sensor lepas: ADC di luar range atau spread sangat besar
  bool soilLepas = (soilAdc < 500) || (soilAdc > 4000) ||
                   (soilAdc >= DRY_VALUE - 100) || (soilSpread > 300);
  int soil = 0;
  if (!soilLepas) {
    soil = (int)constrain(map(soilAdc, DRY_VALUE - 100, WET_VALUE, 0, 100), 0, 100);
  }

  // Jika tanah benar-benar kering & sensor lepas, pH juga dianggap tidak valid
  if (soil < 5 && soilLepas) phOk = false;

  // ── 4. Filter EMA pada pH + koreksi bias galvanik ───────────
  static float phEma    = -1.0f;
  static float phFinal  =  0.0f;
  static bool  phValid  = false;

  if (phOk) {
    // Koreksi bias jika 2 probe dalam 1 wadah dan tanah cukup basah
    float phCorr = phRaw;
    if (PH_BIAS > 0.0f && soil >= SOIL_MIN_FOR_BIAS) {
      float adj = phRaw - PH_BIAS;
      if (adj >= 3.0f && adj <= 9.0f) phCorr = adj;
    }
    // EMA alpha=0.3: respon cepat tapi tetap meredam noise
    phEma   = (phEma < 0.0f) ? phCorr : (0.3f * phCorr + 0.7f * phEma);
    phFinal = phEma;
    phValid = true;
  } else {
    phEma   = -1.0f; // reset EMA saat sensor lepas
    phFinal = 0.0f;
    phValid = false;
  }

  // ── 5. Fuzzy Tahani → skor centroid 0–1 ─────────────────────
  float scoreSuhu = dhtReady ? fuzzyTemp(t)          : 0.0f;
  float scoreSoil =            fuzzySoil(soil);
  float scorePh   = phValid  ? fuzzyPh(phFinal)      : 0.0f;

  // ── 6. Keputusan aktuator (ON jika skor >= FUZZY_THRESHOLD) ─
  bool kipasOn = dhtReady && (scoreSuhu >= FUZZY_THRESHOLD);
  bool airOn   =             (scoreSoil >= FUZZY_THRESHOLD);
  bool phOn    = phValid  && (scorePh   >= FUZZY_THRESHOLD);

  // Tulis relay hanya saat status berubah (kurangi bouncing)
  relayUpdate(RELAY_KIPAS_PIN, kipasOn, prevKipas);
  relayUpdate(RELAY_AIR_PIN,   airOn,   prevAir);
  relayUpdate(RELAY_PH_PIN,    phOn,    prevPh);

  // ── 7. Serial Monitor ────────────────────────────────────────
  float phDisplay = phValid ? roundf(phFinal * 10.0f) / 10.0f : 0.0f;
  Serial.printf("%s T=%.1fC H=%.0f%% Soil=%d%%(adc=%d) pH=%s(adc=%d sp=%d) Kipas=%d Air=%d pH=%d\n",
    getTimeStr().c_str(), t, h, soil, soilAdc,
    phValid ? String(phDisplay, 1).c_str() : "--", phAdc, phSpread,
    kipasOn, airOn, phOn);

  // ── 8. Publish MQTT (tiap MQTT_INTERVAL_MS) ──────────────────
  unsigned long now = millis();
  if (now - lastMqttMs >= MQTT_INTERVAL_MS) {
    lastMqttMs = now;
    publishMqtt(t, h, soil, phFinal, phValid,
                scoreSuhu, scoreSoil, scorePh,
                kipasOn, airOn, phOn);
  }

  // ── 9. Insert Supabase (tiap SUPABASE_INTERVAL_MS) ───────────
  if (now - lastSupabaseMs >= SUPABASE_INTERVAL_MS) {
    lastSupabaseMs = now;
    insertSupabase(t, h, soil, phFinal, phValid,
                   scoreSuhu, scoreSoil, scorePh,
                   kipasOn, airOn, phOn);
  }

  waitMqtt(3000); // jeda 3 detik sebelum siklus berikutnya
}
