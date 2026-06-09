/*
 * Sistem Monitoring & Kontrol Greenhouse CABAI RAWIT — ESP32
 * (Capsicum frutescens)
 *
 * Parameter optimal cabai rawit:
 *   - Kelembaban tanah : 50%-70%
 *   - pH tanah        : 6-7
 *   - Suhu udara      : 24-28°C (DHT22)
 *
 * Kontrol: FUZZY TAHANI, 3 jalur terpisah — selaras grafik.py & index.html
 * -------------------------------------------------------------------------
 * Tahapan per jalur:
 *   1. Fuzzifikasi input (μ) — kurva di grafik_fuzzy_input.png
 *   2. Aturan IF-THEN (lihat komentar tiap fuzzyXxxFrom...)
 *   3. Implikasi MIN, agregasi MAX
 *   4. Defuzzifikasi centroid → skor 0–1 (grafik_fuzzy_output.png)
 *   5. Aktuator ON jika skor >= RELAY_FUZZY_THRESHOLD (0,5)
 *
 * Jalur aktuator:
 *   - fuzzy_suhu  ← suhu DHT22     → relay blower GPIO25 (kolom DB relay_blower = flag 0/1)
 *   - fuzzy_soil  ← kelembaban %  → relay air GPIO26
 *   - fuzzy_ph    ← pH tanah       → relay pH GPIO27
 *
 * Sensor: DHT22 GPIO4 | soil AO GPIO35 | pH ADC GPIO34, DMS GPIO13
 * 1 wadah: pH dulu → tanah → koreksi bias pH jika probe soil basah (PH_BIAS_WET_SOIL).
 * Probe pH dicabut: ADC putus/mengambang → ph_valid=0, tampilan "--" (tanpa hold nilai lama).
 * Hardware terbaik: MOSFET matikan VCC modul soil saat baca pH (SOIL_PWR_PIN).
 * Cloud: MQTT topic pertanian/sensor (~10 s) | Supabase tabel pertanian (~30 s)
 * Daya: sensor & servo 3,3 V | modul relay 5 V | adaptor 12 V → expansion board
 *
 * Dashboard index.html: skor Tahani centroid; hijau jika >= 0,5 (sama ambang firmware).
 */

 #include <DHT.h>
 #include <WiFi.h>
 #include <WiFiClientSecure.h>
 #include <PubSubClient.h>
 #include <HTTPClient.h>
 #include <ArduinoJson.h>
 
 // ==========================
 // Konfigurasi Sensor DHT22 (AM2302)
 // ==========================
 // Modul 3 pin (VCC, DATA, GND) dengan PCB — pull-up ~10 kΩ sudah di modul, tanpa resistor ekstra.
// Suhu optimal 24-28°C; jalur fuzzy suhu → relay blower (grafik.py: Rendah/Sedang/Tinggi °C).
 const float TEMP_OPTIMAL_MIN = 24.0f;
 const float TEMP_OPTIMAL_MAX = 28.0f;
 const int DHTPIN  = 4;       // Pin DATA modul DHT22 (GPIO4)
 const int DHTTYPE = DHT22;
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
 // Kolom POST: fuzzy_suhu/soil/ph = skor centroid Tahani 0–1; relay_* = status aktuator
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
 // Konfigurasi Sensor Soil Moisture (modul probe + PCB)
 // ==========================
 // Modul 4 pin: VCC, GND, DO (digital), AO (analog) — firmware pakai AO saja.
 // Probe resistif + potensiometer di PCB; tanpa resistor ekstra di AO jika keluaran 0–3,3 V.
 // - SOIL_PIN  : AO → ADC ESP32 (GPIO35).
 // - DRY_VALUE / WET_VALUE : kalibrasi dari Serial Monitor (kering vs basah).
 const int SOIL_PIN   = 35;   // AO modul soil moisture → ADC
 const int DRY_VALUE  = 3000; // Nilai ADC saat tanah sangat kering (kalibrasi)
 const int WET_VALUE  = 1000; // Nilai ADC saat tanah sangat basah (kalibrasi)
 const int SOIL_SAMPLES = 15;
 const unsigned long SOIL_AFTER_PH_MS = 1500UL; // jeda setelah DMS OFF sebelum baca tanah
 
 // ============
 // Aktuator (hasil Fuzzy Tahani, ambang RELAY_FUZZY_THRESHOLD)
 // ============
 // - Blower: relay GPIO21 (aktif-LOW default).
 // - Air & pH: relay VCC 5 V, aktif-LOW (LOW = ON). Set RELAY_ACTIVE_LOW = false jika modul aktif-HIGH.
 const bool RELAY_ACTIVE_LOW = true;
 const int RELAY_BLOWER_PIN = 25; // relay blower (kipas)
 const int RELAY_WATER_PIN   = 26; // relay pompa air
 const int RELAY_PH_PIN      = 27; // relay koreksi pH 
 
 // Ambang defuzzifikasi Tahani (centroid 0–1) → ON/OFF aktuator
 const float RELAY_FUZZY_THRESHOLD = 0.5f;
 
 // =============================
 // Konfigurasi Sensor pH Tanah (probe + driver DMS)
 // =============================
 // - DMSpin    : aktifkan modul kondisioner sinyal (GPIO13).
 // - PH_ADC_PIN: keluaran analog pH → ADC (GPIO34, hanya input).
 // Jika keluaran modul pH sampai 5 V: pasang pembagi 10 kΩ (ke sinyal) + 20 kΩ (ke GND) sebelum GPIO34.
 // Jika keluaran sudah 0–3,3 V: tanpa resistor pembagi.
 const int DMSpin       = 13; // kabel biru
 const int PH_ADC_PIN   = 34; // kabel ungu
 const int PH_SAMPLES   = 25;
 // Stabilisasi DMS setelah LOW (aktif): tunggu PH_SETTLE_MS + flush + 25 sampel (~250 ms).
 // Total fase baca pH per siklus ≈ 8,3 detik; setelah pasang probe tunggu ≥1 siklus penuh (~13 s).
 const unsigned long PH_SETTLE_MS = 8000UL;
 const int PH_ADC_MIN   = 200;   // ADC terlalu rendah (gangguan)
 const int PH_ADC_MAX   = 3800;  // ADC terlalu tinggi
 const int PH_ADC_DISCONNECT_LO = 80;   // pin mengambang / probe lepas ke GND
 const int PH_ADC_DISCONNECT_HI = 4050; // pin mengambang / probe lepas
 const int PH_SPREAD_MAX = 220;  // spread ADC besar = kabel lepas / noise
const uint8_t PH_HOLD_MAX_INVALID = 2; // tahan hingga 2 siklus invalid singkat
 // Koreksi software 1 wadah: probe soil basah menaikkan pembacaan pH ~0,5–1,0.
 // Uji: catat pH hanya-probe vs dua-probe, sesuaikan (mis. 8,1→9,0 → set 0,9).
 const float PH_BIAS_WET_SOIL = 0.9f;
 const int SOIL_WET_PERCENT_MIN = 35; // % tanah: di atas ini koreksi bias dipakai
 // Opsional: GPIO25 + MOSFET ke VCC modul soil (-1 = tidak dipakai). Paling akurat untuk 1 wadah.
 const int SOIL_PWR_PIN = -1;
 
 int   PH_ADC;          // nilai ADC mentah untuk pH
 int   PH_SPREAD;       // sebaran sampel ADC pH
 float pH_value;        // nilai pH saat ini
 bool  phOkSiklus;     // pembacaan siklus ini valid (3–9, ADC wajar)
 bool  phDikoreksi;    // true jika bias 1-wadah diterapkan
 bool  phTampilValid;  // boleh ditampilkan MQTT/dashboard
 
 static float lastGoodPh = 0.0f;
 static bool hasLastGoodPh = false;
 static uint8_t phInvalidStreak = 0;
 
 static void adcFlushPin(int pin, int n) {
   for (int i = 0; i < n; i++) {
     (void)analogRead(pin);
     delay(5);
   }
 }
 
 static int bacaAdcRata(int pin, int nSamples, int &spread) {
   long sum = 0;
   int vmin = 4095;
   int vmax = 0;
   for (int i = 0; i < nSamples; i++) {
     int v = analogRead(pin);
     sum += v;
     if (v < vmin) vmin = v;
     if (v > vmax) vmax = v;
     delay(10);
   }
   spread = vmax - vmin;
   return (int)(sum / nSamples);
 }
 
 static float adcKePh(int adc) {
   float adc10bit = (float)adc / 4.0f;
   return (-0.0233f * adc10bit) + 12.698f;
 }
 
 /** Probe/kabel lepas: ADC mendekati 0 atau 4095, atau sampel sangat berisik. */
 static bool phProbeTerputus(int adc, int spread) {
   if (adc <= PH_ADC_DISCONNECT_LO || adc >= PH_ADC_DISCONNECT_HI) return true;
   if (spread > PH_SPREAD_MAX) return true;
   return false;
 }
 
 static bool phPembacaanValid(float ph, int adc, int spread) {
   if (phProbeTerputus(adc, spread)) return false;
   if (adc < PH_ADC_MIN || adc > PH_ADC_MAX) return false;
   return (ph >= 3.0f && ph <= 9.0f);
 }
 
 static void soilPowerSet(bool on) {
   if (SOIL_PWR_PIN < 0) return;
   pinMode(SOIL_PWR_PIN, OUTPUT);
   digitalWrite(SOIL_PWR_PIN, on ? HIGH : LOW);
 }
 
 /** Kurangi bias pH saat probe soil ikut di media basah (setelah % tanah diketahui). */
 static float koreksiPhSatuWadah(float phRaw, int moisturePercent, bool &applied) {
   applied = false;
   if (SOIL_PWR_PIN >= 0) return phRaw;
   if (moisturePercent < SOIL_WET_PERCENT_MIN) return phRaw;
   float adj = phRaw - PH_BIAS_WET_SOIL;
   if (adj < 3.0f || adj > 9.0f) return phRaw;
   applied = true;
   return adj;
 }
 
 // =============================================================================
 // LOGIKA FUZZY TAHANI (3 jalur — selaras grafik.py)
 // =============================================================================
 
 static float fminfz(float a, float b) { return (a < b) ? a : b; }
 static float fmaxfz(float a, float b) { return (a > b) ? a : b; }
 
 // trimf / trapmf: fungsi keanggotaan input & output (fuzzifikasi Tahani)
 static float trimf(float x, float a, float b, float c) {
   if ((a == b) && (x == a)) return 1.0f;
   if ((b == c) && (x == c)) return 1.0f;
   if (x <= a || x >= c) return 0.0f;
   if (x == b) return 1.0f;
   if (x < b) return (x - a) / (b - a);
   return (c - x) / (c - b);
 }
 
 static float trapmf(float x, float a, float b, float c, float d) {
   if ((a == b) && (x == a)) return 1.0f;
   if ((c == d) && (x == d)) return 1.0f;
   if (x <= a || x >= d) return 0.0f;
   if (x >= b && x <= c) return 1.0f;
   if (x > a && x < b) return (x - a) / (b - a);
   return (d - x) / (d - c);
 }
 
 enum OutKind { OUT_RENDAH = 0, OUT_SEDANG = 1, OUT_TINGGI = 2 };
 
 // Tabel pencarian untuk keanggotaan output guna mempercepat komputasi (menghilangkan division/math di loop)
 static float out_mu_table[3][21];
 static bool table_initialized = false;
 
 static void initFuzzyTable() {
   for (int i = 0; i <= 20; i++) {
     float y = (float)i * 0.05f;
     out_mu_table[OUT_RENDAH][i] = trapmf(y, 0.0f, 0.0f, 0.15f, 0.45f);
     out_mu_table[OUT_SEDANG][i] = trimf(y, 0.15f, 0.35f, 0.55f);
     out_mu_table[OUT_TINGGI][i] = trapmf(y, 0.45f, 0.65f, 1.0f, 1.0f);
   }
   table_initialized = true;
 }
 
 /** Defuzzifikasi centroid Fuzzy Tahani (implikasi MIN, agregasi MAX) yang dioptimalkan dengan tabel pencarian. */
 static float fuzzyTahaniCentroid(float mu1, int kind1, float mu2, int kind2, float mu3, int kind3) {
   if (!table_initialized) {
     initFuzzyTable();
   }
   float num = 0.0f, den = 0.0f;
   for (int i = 0; i <= 20; i++) {
     float y = (float)i * 0.05f; // Menggantikan pembagian berat dengan perkalian konstan
     float agg = 0.0f;
     agg = fmaxfz(agg, fminfz(mu1, out_mu_table[kind1][i]));
     agg = fmaxfz(agg, fminfz(mu2, out_mu_table[kind2][i]));
     agg = fmaxfz(agg, fminfz(mu3, out_mu_table[kind3][i]));
     num += y * agg;
     den += agg;
   }
   return (den > 1e-6f) ? (num / den) : 0.0f;
 }
 
 // -----------------------------------------------------------------------------
 // JALUR 1 — SUHU (DHT22) → fuzzy_suhu → relay blower
 // -----------------------------------------------------------------------------
 static float fuzzyParanetFromTemp(float tempC, bool tempValid) {
   if (!tempValid) return 0.0f;
   float muR = trapmf(tempC, 0.0f, 0.0f, 24.0f, 27.0f);
   float muS = trimf(tempC, 24.0f, 27.0f, 31.0f);
   float muT = trapmf(tempC, 27.0f, 31.0f, 45.0f, 45.0f);
   return fuzzyTahaniCentroid(muR, OUT_RENDAH, muS, OUT_SEDANG, muT, OUT_TINGGI);
 }
 
 // -----------------------------------------------------------------------------
 // JALUR 2 — TANAH (%) → fuzzy_soil → relay air
 // -----------------------------------------------------------------------------
 static float fuzzyWaterFromSoil(int moisturePercent) {
   float x = (float)moisturePercent;
   float muK = trapmf(x, 0.0f, 0.0f, 40.0f, 50.0f);
   float muL = trapmf(x, 40.0f, 50.0f, 70.0f, 80.0f);
   float muB = trapmf(x, 70.0f, 80.0f, 100.0f, 100.0f);
   return fuzzyTahaniCentroid(muK, OUT_TINGGI, muL, OUT_RENDAH, muB, OUT_RENDAH);
 }
 
 // -----------------------------------------------------------------------------
 // JALUR 3 — pH → fuzzy_ph → relay koreksi larutan
 // -----------------------------------------------------------------------------
 static float fuzzyPhCorrectionFromPh(float ph, bool phValid) {
   if (!phValid) return 0.0f;
   float muA = trapmf(ph, 3.0f, 3.0f, 5.0f, 6.0f);
   float muN = trapmf(ph, 5.5f, 6.0f, 7.0f, 7.5f);
   float muB = trapmf(ph, 7.0f, 7.5f, 9.0f, 9.0f);
   return fuzzyTahaniCentroid(muA, OUT_TINGGI, muN, OUT_RENDAH, muB, OUT_TINGGI);
 }
 
 static void relayWrite(int pin, bool on) {
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
   float ph, float fuzzy_suhu, float fuzzy_soil, float fuzzy_ph,
   bool blower_on, bool relay_air, bool relay_ph
 ) {
   if (WiFi.status() != WL_CONNECTED) return false;
 
   HTTPClient http;
   String url = String(supabaseUrl) + "/rest/v1/" + tableName;
   http.begin(url);
   http.addHeader("apikey", supabaseKey);
   http.addHeader("Authorization", String("Bearer ") + supabaseKey);
   http.addHeader("Content-Type", "application/json");
   http.addHeader("Prefer", "return=minimal");
 
   // Menggunakan StaticJsonDocument untuk efisiensi memori RAM dan mencegah fragmentasi heap
   StaticJsonDocument<384> doc;
   doc["temperature"] = temperature;
   doc["humidity"] = humidity;
   doc["soil"] = soil;
   doc["ph"] = ph;
   doc["fuzzy_suhu"] = fuzzy_suhu;
   doc["fuzzy_soil"] = fuzzy_soil;
   doc["fuzzy_ph"] = fuzzy_ph;
   doc["relay_blower"] = blower_on ? 1 : 0;
   doc["relay_air"] = relay_air ? 1 : 0;
   doc["relay_dolomit"] = relay_ph ? 1 : 0;
 
   char body[384];
   serializeJson(doc, body, sizeof(body));
 
   int code = http.POST((uint8_t*)body, strlen(body));
 
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
 
   analogReadResolution(12);
   analogSetAttenuation(ADC_11db);
   analogSetPinAttenuation(SOIL_PIN, ADC_11db);
   analogSetPinAttenuation(PH_ADC_PIN, ADC_11db);
 
   pinMode(DMSpin, OUTPUT);
   digitalWrite(DMSpin, HIGH); // DMS nonaktif (HIGH)
   soilPowerSet(true);
   if (SOIL_PWR_PIN >= 0) {
     pinMode(SOIL_PWR_PIN, OUTPUT);
     digitalWrite(SOIL_PWR_PIN, HIGH);
   }
 
  // Relay blower + relay air / pH
  pinMode(RELAY_BLOWER_PIN, OUTPUT);
  relayWrite(RELAY_BLOWER_PIN, false);
   pinMode(RELAY_WATER_PIN, OUTPUT);
   pinMode(RELAY_PH_PIN, OUTPUT);
   relayWrite(RELAY_WATER_PIN, false);
   relayWrite(RELAY_PH_PIN, false);
 
   dht.begin();
 
   ensureWiFi();
   ensureMqtt();
 
   Serial.println("Greenhouse cabai rawit — Fuzzy Tahani (Mamdani) — monitoring IoT");
 }
 
 void loop() {
   ensureWiFi();
   ensureMqtt();
   mqttClient.loop();
 
    // Baca DHT22 (min. ~2 s antar pembacaan disarankan)
    // Stabilisasi dengan filter Exponential Moving Average (EMA) untuk meredam noise
    static float filtered_t = 26.0f; // Default suhu optimal
    static float filtered_h = 60.0f; // Default kelembaban optimal
    static bool dhtInitialized = false;

    float h_raw = dht.readHumidity();
    float t_raw = dht.readTemperature();
    bool dhtOk = !(isnan(h_raw) || isnan(t_raw));
  
    if (dhtOk) {
      if (!dhtInitialized) {
        filtered_t = t_raw;
        filtered_h = h_raw;
        dhtInitialized = true;
      } else {
        // EMA filter (alpha = 0.15) untuk meredam fluktuasi
        filtered_t = (0.15f * t_raw) + (0.85f * filtered_t);
        filtered_h = (0.15f * h_raw) + (0.85f * filtered_h);
      }
    } else {
      Serial.println("DHT22 read error! Menggunakan nilai terfilter terakhir.");
    }
    float t = filtered_t;
    float h = filtered_h;
 
   // ===== pH dulu — matikan VCC soil jika SOIL_PWR_PIN di-wire =====
   phDikoreksi = false;
   soilPowerSet(false);
   digitalWrite(DMSpin, LOW);
   waitWithMqtt(PH_SETTLE_MS);
   adcFlushPin(PH_ADC_PIN, 6);
   PH_ADC = bacaAdcRata(PH_ADC_PIN, PH_SAMPLES, PH_SPREAD);
  float pH_raw = adcKePh(PH_ADC);
  // Untuk mengurangi false-positive pada batas atas (≈9.0), bulatkan
  // pH ke 1 desimal sebelum validasi. Contoh: 9.005 -> 9.0
  float pH_for_check = roundf(pH_raw * 10.0f) / 10.0f;
  phOkSiklus = phPembacaanValid(pH_for_check, PH_ADC, PH_SPREAD);
 
   digitalWrite(DMSpin, HIGH);
   waitWithMqtt(SOIL_AFTER_PH_MS);
   soilPowerSet(true);
 
    // ===== Kelembaban tanah =====
    adcFlushPin(SOIL_PIN, 4);
    int soilSpread = 0;
    int soilAdc = bacaAdcRata(SOIL_PIN, SOIL_SAMPLES, soilSpread);
    int moisturePercent_raw = map(soilAdc, DRY_VALUE, WET_VALUE, 0, 100);
    moisturePercent_raw = constrain(moisturePercent_raw, 0, 100);

    // Stabilisasi kelembaban tanah dengan filter EMA (alpha = 0.15)
    static float filtered_soil = -1.0f;
    if (filtered_soil < 0.0f) {
      filtered_soil = (float)moisturePercent_raw;
    } else {
      filtered_soil = (0.15f * (float)moisturePercent_raw) + (0.85f * filtered_soil);
    }
    int moisturePercent = (int)roundf(filtered_soil);
    static float filtered_ph = -1.0f;
    float phPakai = pH_raw;
    if (phOkSiklus) {
      float phKoreksi = koreksiPhSatuWadah(pH_raw, moisturePercent, phDikoreksi);
      if (filtered_ph < 0.0f) {
        filtered_ph = phKoreksi;
      } else {
        // EMA filter (alpha = 0.15) untuk meredam noise pembacaan pH
        filtered_ph = (0.15f * phKoreksi) + (0.85f * filtered_ph);
      }
      phPakai = filtered_ph;
    }
  
    if (phOkSiklus) {
      phInvalidStreak = 0;
      lastGoodPh = phPakai;
      hasLastGoodPh = true;
      pH_value = phPakai;
      phTampilValid = true;
    } else {
      phInvalidStreak++;
      if (PH_HOLD_MAX_INVALID > 0 && hasLastGoodPh && phInvalidStreak <= PH_HOLD_MAX_INVALID) {
        pH_value = lastGoodPh;
        phTampilValid = true;
      } else {
        hasLastGoodPh = false;
        pH_value = 0.0f;
        phTampilValid = false;
      }
    }

    // ===== Defuzzifikasi Tahani (centroid 0–1), aktuator ON jika >= RELAY_FUZZY_THRESHOLD =====
    // Kontrol fuzzy/relay menggunakan nilai terfilter yang stabil.
    float ph = phOkSiklus ? phPakai : 0.0f;
    bool phValid = phOkSiklus && (ph >= 3.0f && ph <= 9.0f);
 
    float scoreParanet = fuzzyParanetFromTemp(t, dhtInitialized);       // → fuzzy_suhu (menggunakan validitas dhtInitialized)
    float scoreSoil = fuzzyWaterFromSoil(moisturePercent);              // → fuzzy_soil
    float scorePh = fuzzyPhCorrectionFromPh(ph, phValid);              // → fuzzy_ph
 
    bool blowerOn = dhtInitialized && (scoreParanet >= RELAY_FUZZY_THRESHOLD);
    bool waterOn = (scoreSoil >= RELAY_FUZZY_THRESHOLD);
    bool phRelayOn = phValid && (scorePh >= RELAY_FUZZY_THRESHOLD);
 
    relayWrite(RELAY_BLOWER_PIN, blowerOn);
    relayWrite(RELAY_WATER_PIN, waterOn);
    relayWrite(RELAY_PH_PIN, phRelayOn);
 
   // ===== Ringkasan singkat ke Serial Monitor (1 baris per loop) =====
   float phRounded = phTampilValid ? roundf(pH_value * 10.0f) / 10.0f : 0.0f;
   Serial.print("T=");
   Serial.print(t, 1);
   Serial.print("C H=");
   Serial.print(h, 0);
   Serial.print("% Soil=");
   Serial.print(moisturePercent);
   Serial.print("% SoilAdc=");
   Serial.print(soilAdc);
   Serial.print("% pH=");
   if (phTampilValid) {
     Serial.print(phRounded, 1);
   } else {
     Serial.print("--");
   }
   Serial.print(phOkSiklus ? "" : (phTampilValid ? "(hold)" : "(putus)"));
   Serial.print(phDikoreksi ? "(adj)" : "");
   Serial.print(" PhAdc=");
   Serial.print(PH_ADC);
   Serial.print(" PhSp=");
   Serial.print(PH_SPREAD);
   Serial.print(" P=");
  Serial.print(blowerOn ? 1 : 0);
   Serial.print(" W=");
   Serial.print(waterOn ? 1 : 0);
   Serial.print(" H=");
   Serial.print(phRelayOn ? 1 : 0);
   Serial.println();
 
   // ===== Publish MQTT =====
   unsigned long now = millis();
   if (mqttClient.connected() && (now - lastMqttPubMs >= MQTT_PUB_INTERVAL_MS)) {
     lastMqttPubMs = now;
 
     StaticJsonDocument<384> doc;
     doc["temperature"] = t;
     doc["humidity"] = h;
     doc["soil"] = moisturePercent;
     doc["ph_valid"] = phTampilValid ? 1 : 0;
     doc["ph"] = phTampilValid ? phRounded : 0.0f;
     // Skor centroid Tahani 0–1 (index.html, ambang tampilan 0,5)
     doc["fuzzy_suhu"] = scoreParanet;
     doc["fuzzy_soil"] = scoreSoil;
     doc["fuzzy_ph"] = scorePh;
    doc["relay_blower"] = blowerOn ? 1 : 0;
     doc["relay_air"] = waterOn ? 1 : 0;
     doc["relay_ph"] = phRelayOn ? 1 : 0;
     doc["relay_dolomit"] = phRelayOn ? 1 : 0; // kompatibel nama lama = relay koreksi pH
     doc["wifi_connected"] = wifiConnectedFlag ? 1 : 0;
     doc["wifi_ip"] = wifiIp;
     doc["temp_optimal"] = (t >= TEMP_OPTIMAL_MIN && t <= TEMP_OPTIMAL_MAX) ? 1 : 0;
 
     char buffer[384];
     size_t n = serializeJson(doc, buffer, sizeof(buffer));
     mqttClient.publish(topic_pub, buffer, n);
   }
 
   // ===== Kirim ke Supabase =====
   if (now - lastSupabaseMs >= SUPABASE_INTERVAL_MS) {
     lastSupabaseMs = now;
     supabaseInsert(t, h, moisturePercent,
                    phTampilValid ? phRounded : 0.0f,
                    scoreParanet, scoreSoil, scorePh,
                    blowerOn, waterOn, phRelayOn);
   }
 
   waitWithMqtt(3UL * 1000UL); // jeda sebelum pembacaan berikutnya (tetap jaga MQTT)
 }