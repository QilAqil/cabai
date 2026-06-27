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
 const unsigned long SUPABASE_INTERVAL_MS = 60UL * 1000UL; // Diubah ke 60 detik (1 menit)
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
  // Kalibrasi sensor kapasitif anti-karat (ESP32 12-bit ADC).
  // Kalibrasi: gantung sensor di udara kering → catat SoilAdc → isi DRY_VALUE.
  //            tancap sensor di tanah sangat basah 5 menit → catat SoilAdc → isi WET_VALUE.
  const int DRY_VALUE  = 3200; // Nilai ADC saat sensor di udara / kering (0%)
  //const int WET_VALUE  = 1834; // (Dikalibrasi ulang) ADC saat ini akan membaca 64%
  const int WET_VALUE  = 150;  // (Dikalibrasi ulang) SoilAdc=1202 → 64%, SoilAdc=150 → 100%
 const int SOIL_SAMPLES = 10; // Jumlah sampel averaging per pembacaan
 const unsigned long SOIL_AFTER_PH_MS   = 2500UL; // jeda setelah DMS OFF + soil power ON sebelum baca (diperpanjang agar ADC stabil)
 const unsigned long SOIL_DISCHARGE_MS  = 3000UL; // jeda setelah soil power OFF sebelum baca pH (1 wadah):
                                                   // memberi waktu arus galvanik di larutan hilang agar
                                                   // elektroda pH tidak terbias oleh potensial probe soil.
 
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
 // - DMSpin      : aktifkan modul kondisioner sinyal DMS (GPIO13, kabel biru).
 // - PH_ADC_PIN  : keluaran analog pH → ADC (GPIO34, kabel ungu).
 // - INDIKATOR_PIN: LED built-in ESP32 (GPIO2) menyala selama fase baca pH.
 // Jika keluaran modul pH sampai 5 V: pasang pembagi 10 kΩ (ke sinyal) + 20 kΩ (ke GND) sebelum GPIO34.
 // Jika keluaran sudah 0–3,3 V: tanpa resistor pembagi.
 const int DMSpin        = 13;      // kabel biru
 const int PH_ADC_PIN    = 34;      // kabel ungu
 const int INDIKATOR_PIN = 2;       // LED built-in ESP32 — menyala saat baca pH
 const int PH_SAMPLES    = 15;      // Jumlah sampel filter Median (ditingkatkan dari 10 ke 15 agar super stabil)
 // DMS aktif (LOW) selama PH_SETTLE_MS agar probe stabil sebelum ADC dibaca.
 const unsigned long PH_SETTLE_MS = 10000UL; // 10 detik (sesuai referensi kode DMS)
 const uint8_t PH_HOLD_MAX_INVALID = 1; // (Diturunkan) Hanya tahan 1 siklus saja agar respon "putus" di layar lebih cepat
 const int PH_ADC_MIN     = 200;     // ADC terlalu rendah (gangguan / terputus)
 const int PH_ADC_MAX     = 3800;    // ADC terlalu tinggi (terputus)
 const int PH_SPREAD_MAX  = 60;      // (Dinaikkan ke 60) Mengakomodasi noise EMI dari relay aktif; PhSp=47-50 masih dianggap valid.
 // Koreksi software 1 wadah: offset pH akibat interferensi galvanik probe soil.
 // Saat 2 probe dalam 1 wadah, probe soil menginjeksikan arus ke media → pH terbaca
 // lebih basa dari nilai sebenarnya. Kalibrasi: ukur pH dengan alat referensi manual,
 // lalu isi PH_BIAS_WET_SOIL = (pH_terbaca - pH_referensi).
 // Contoh log: pH terbaca 7.8, pH manual ~5.9 → bias = 1.9 → isi 1.9f.
 // Set ke 0.0f jika probe soil dan pH di wadah TERPISAH (tidak ada interferensi).
 const float PH_BIAS_WET_SOIL = 1.9f;  // offset positif karena pH terbias naik
 const int SOIL_WET_PERCENT_MIN = 35; // % tanah: di atas ini koreksi bias dipakai
 // Sensor logam WAJIB menggunakan power gating agar bebas dari polarisasi galvanik.
 // Sambungkan kabel VCC sensor tanah ke GPIO14 (bukan langsung ke 3.3V/5V).
 const int SOIL_PWR_PIN = 14; // GPIO14 = saklar daya sensor tanah
 
 int   PH_ADC;          // nilai ADC mentah untuk pH
 float pH_value;        // nilai pH saat ini
 bool  phOkSiklus;     // pembacaan siklus ini valid (pH <= 14.0)
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
 
 // ALGORITMA BARU: Filter Median
 // Sangat ampuh menstabilkan sensor dengan cara membuang angka-angka ekstrem (noise/spike)
 static int bacaAdcMedian(int pin, int nSamples, int &spread) {
   if (nSamples > 30) nSamples = 30; // Batas aman array
   int values[30];
   
   for (int i = 0; i < nSamples; i++) {
     values[i] = analogRead(pin);
     delay(5);
   }
   
   // Urutkan nilai (Bubble Sort) dari terkecil ke terbesar
   for (int i = 0; i < nSamples - 1; i++) {
     for (int j = 0; j < nSamples - i - 1; j++) {
       if (values[j] > values[j+1]) {
         int temp = values[j];
         values[j] = values[j+1];
         values[j+1] = temp;
       }
     }
   }
   
   // Hitung spread dengan mengabaikan nilai paling ekstrem (ujung atas dan bawah)
   // Ini mencegah sensor divonis "putus" hanya karena 1 kedipan listrik
   if (nSamples >= 5) {
     spread = values[nSamples - 2] - values[1];
   } else {
     spread = values[nSamples - 1] - values[0];
   }
   
   // Ambil nilai paling tengah (Median) yang dijamin bersih dari noise
   return values[nSamples / 2];
 }
 
 static float adcKePh(int adc) {
   float adc10bit = (float)adc / 4.0f;
   // Kalibrasi ulang: ADC 845 = pH 7.0 (disesuaikan dengan alat manual)
   return (-0.0233f * adc10bit) + 11.922f;
 }
 
  static bool phPembacaanValid(float ph, int adc, int spread) {
    if (adc < PH_ADC_MIN || adc > PH_ADC_MAX) return false;
    if (spread > PH_SPREAD_MAX) return false;
    // Maksimal pH tanah yang diizinkan adalah 9.0
    return (ph >= 3.0f && ph <= 9.0f);
  }
 
 static void soilPowerSet(bool on) {
   if (SOIL_PWR_PIN < 0) return;
   pinMode(SOIL_PWR_PIN, OUTPUT);
   digitalWrite(SOIL_PWR_PIN, on ? HIGH : LOW);
 }
 
 /** Kurangi bias pH akibat interferensi galvanik probe soil (skenario 1 wadah).
  *  Bias terjadi karena arus probe soil mengubah potensial referensi elektroda pH.
  *  Fungsi ini mengurangi offset bias jika tanah cukup basah (probe soil terendam).
  *  @param phRaw          nilai pH mentah dari ADC (setelah filter median + EMA)
  *  @param moisturePercent kelembaban tanah saat ini (%)
  *  @param applied         di-set true jika koreksi diterapkan
  *  @returns nilai pH setelah koreksi, atau phRaw jika koreksi tidak applicable
  */
 static float koreksiPhSatuWadah(float phRaw, int moisturePercent, bool &applied) {
   applied = false;
   // Hanya koreksi jika PH_BIAS_WET_SOIL dikonfigurasi (> 0) DAN tanah cukup basah
   // (memastikan probe soil benar-benar terendam larutan sehingga bias terjadi).
   if (PH_BIAS_WET_SOIL <= 0.0f) return phRaw;
   if (moisturePercent < SOIL_WET_PERCENT_MIN) return phRaw;
   float adj = phRaw - PH_BIAS_WET_SOIL;
   if (adj < 3.0f || adj > 9.0f) return phRaw; // jaga batas pH valid
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
   doc["relay_kipas"] = blower_on ? 1 : 0;
   doc["relay_air"] = relay_air ? 1 : 0;
   doc["relay_ph"] = relay_ph ? 1 : 0;
 
   char body[384];
   serializeJson(doc, body, sizeof(body));
 
   int code = http.POST((uint8_t*)body, strlen(body));
 
   if (code < 200 || code >= 300) {
     Serial.print("Supabase POST Error: ");
     Serial.print(code);
     Serial.print(" - ");
     Serial.println(http.getString());
   }
 
   http.end();
   return (code >= 200 && code < 300);
 }

 String getTimeStr() {
   struct tm timeinfo;
   if (!getLocalTime(&timeinfo, 10)) {
     // Jika belum dapat jam internet, gunakan hitungan waktu hidup alat
     unsigned long s = millis() / 1000;
     char buf[16];
     sprintf(buf, "[%02lu:%02lu:%02lu]", (s / 3600), (s / 60) % 60, s % 60);
     return String(buf);
   }
   char buf[32];
   strftime(buf, sizeof(buf), "[%H:%M:%S]", &timeinfo);
   return String(buf);
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
   // Selalu nonaktifkan DMS secara default untuk mencegah elektroda terpolarisasi 
   // yang menyebabkan pembacaan pH naik bertahap (drifting).
   digitalWrite(DMSpin, HIGH);
   pinMode(INDIKATOR_PIN, OUTPUT);
   digitalWrite(INDIKATOR_PIN, LOW);
   if (SOIL_PWR_PIN >= 0) {
     pinMode(SOIL_PWR_PIN, OUTPUT);
     digitalWrite(SOIL_PWR_PIN, LOW); // Mulai dalam keadaan mati (OFF) untuk mencegah korosi awal
   }
 
   pinMode(RELAY_BLOWER_PIN, OUTPUT);
   relayWrite(RELAY_BLOWER_PIN, false);
   pinMode(RELAY_WATER_PIN, OUTPUT);
   pinMode(RELAY_PH_PIN, OUTPUT);
   relayWrite(RELAY_WATER_PIN, false);
   relayWrite(RELAY_PH_PIN, false);
 
   dht.begin();
 
   ensureWiFi();
   configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // Sinkronisasi jam internet (WIB = GMT+7)
   ensureMqtt();
 
   // ===== Warm-up sensor tanah resistif logam =====
   // Sensor logam memerlukan arus stabil beberapa saat untuk menghilangkan polarisasi awal.
   // Caranya: baca dan buang 60 sampel dummy (= ~3 detik) agar ADC mencapai kondisi tunak.
   // Setelah ini selesai, nilai pertama di loop() dijamin sudah stabil.
   Serial.print("Warming up soil sensor");
   for (int i = 0; i < 60; i++) {
     (void)analogRead(SOIL_PIN);
     delay(50);
     if (i % 10 == 9) Serial.print(".");
   }
   Serial.println(" OK");
 
   Serial.println("Greenhouse cabai rawit");
 }
 
 void loop() {
   ensureWiFi();
   ensureMqtt();
   mqttClient.loop();
 
    // Baca DHT22 langsung — sensor digital sudah stabil, tidak perlu filter EMA
    static bool dhtInitialized = false;
    float t_raw = dht.readTemperature();
    float h_raw = dht.readHumidity();
    bool dhtOk = !(isnan(t_raw) || isnan(h_raw));

    static float t = 26.0f, h = 60.0f; // nilai fallback saat error
    if (dhtOk) {
      t = roundf(t_raw * 10.0f) / 10.0f;
      h = roundf(h_raw * 10.0f) / 10.0f;
      dhtInitialized = true;
    } else {
      Serial.println("DHT22 read error! Menggunakan nilai terakhir.");
    }
 
    // ===== pH — aktifkan DMS sesaat, tunggu stabil, lalu baca =====
    // PENTING (1 wadah): Sensor soil WAJIB mati (GPIO14=LOW) SEBELUM DMS aktif
    // dan tetap mati selama seluruh fase baca pH.
    // Alasan: probe soil logam menginjeksikan arus ke media tanah → mengubah
    // potensial referensi elektroda pH → bacaan pH menjadi lebih basa (offset +).
    // Jeda SOIL_DISCHARGE_MS diperlukan agar arus residual di larutan habis
    // sebelum elektroda pH mulai membaca.
    phDikoreksi = false;
    if (SOIL_PWR_PIN >= 0) {
      soilPowerSet(false);                // matikan soil terlebih dahulu
      waitWithMqtt(SOIL_DISCHARGE_MS);    // tunggu arus residual larutan habis
    }
    
    digitalWrite(DMSpin, LOW); // Aktifkan DMS HANYA saat akan membaca
    digitalWrite(INDIKATOR_PIN, HIGH);
    
    // Tunggu 2 detik agar sensor stabil sebelum dibaca
    waitWithMqtt(2000);

    adcFlushPin(PH_ADC_PIN, 6);
    static int phSpread = 0;
    PH_ADC = bacaAdcMedian(PH_ADC_PIN, PH_SAMPLES, phSpread);
    float pH_raw = adcKePh(PH_ADC);
    if (pH_raw > 9.0f) pH_raw = 9.0f; // Batasi maksimal ke 9.0 agar tidak terputus
    if (pH_raw < 3.0f) pH_raw = 3.0f; // Batasi minimal ke 3.0
    phOkSiklus = phPembacaanValid(pH_raw, PH_ADC, phSpread);
    
    // Segera matikan DMS untuk mencegah pembacaan naik perlahan (polarisasi)
    digitalWrite(DMSpin, HIGH);
    digitalWrite(INDIKATOR_PIN, LOW);

    if (SOIL_PWR_PIN >= 0) {
      waitWithMqtt(SOIL_AFTER_PH_MS);
      soilPowerSet(true);
      // Buang sampel awal setelah power gating dinyalakan:
      // GPIO14 baru HIGH → kapasitor modul butuh ~500 ms untuk charge penuh.
      // 10 sampel × 5 ms delay = 50 ms flush, ditambah jeda waitWithMqtt di atas.
      adcFlushPin(SOIL_PIN, 10);
    }
 
     // ===== Kelembaban tanah — Langsung baca, map, constrain =====
    int soilSpread = 0;
    int soilAdc = bacaAdcMedian(SOIL_PIN, SOIL_SAMPLES, soilSpread);
    int moisturePercent = 0;
    
     // Deteksi sensor dicabut: 
    // 1. ADC terlalu rendah/tinggi → floating/short
    // 2. SoilAdc >= DRY_VALUE → sensor di udara kering
    // 3. soilSpread > 300 → ADC pin mengambang dengan noise besar (ambang dinaikkan;
    //    warm-up GPIO14 sesaat menyebabkan spread sementara tinggi meski hardware normal)
    if (soilAdc < 500 || soilAdc > 4000 || soilAdc >= (DRY_VALUE - 100) || soilSpread > 300) {
      moisturePercent = 0;
    } else {
      moisturePercent = map(soilAdc, DRY_VALUE - 100, WET_VALUE, 0, 100);
      moisturePercent = constrain(moisturePercent, 0, 100);
    }
    
    // Jika sensor tanah mendeteksi kering kerontang (< 5%) DAN ADC soil memang
    // benar-benar di luar rentang valid (bukan sekadar spread tinggi), barulah
    // pH dianggap tidak di tanah. Ini mencegah pH diinvalidasi hanya karena
    // noise transien sesaat di sensor tanah.
    bool soilBenarKering = (soilAdc < 500 || soilAdc > 4000 || soilAdc >= (DRY_VALUE - 100));
    if (moisturePercent < 5 && soilBenarKering) {
      phOkSiklus = false;
    }

    static float filtered_ph = -1.0f;
    float phPakai = pH_raw;
    if (phOkSiklus) {
      float phKoreksi = koreksiPhSatuWadah(pH_raw, moisturePercent, phDikoreksi);
      if (filtered_ph < 0.0f) {
        filtered_ph = phKoreksi;
      } else {
        // EMA filter (alpha = 0.30) untuk respon lebih cepat & stabil
        filtered_ph = (0.30f * phKoreksi) + (0.70f * filtered_ph);
      }
      phPakai = filtered_ph;
    } else {
      filtered_ph = -1.0f; // Reset EMA filter saat sensor dicabut
    }
  
    if (phOkSiklus) {
      phInvalidStreak = 0;
      lastGoodPh = phPakai;
      hasLastGoodPh = true;
      pH_value = phPakai;
      phTampilValid = true;
    } else {
      // Deteksi apakah probe benar-benar terputus secara fisik atau keluar dari tanah
      // phSpread dihapus dari kondisi terputus agar saat terjadi noise sesaat (spread tinggi), 
      // sistem masuk ke mode HOLD (bukan terputus) sehingga relay tidak mati.
      bool phTerputus = (PH_ADC < PH_ADC_MIN || PH_ADC > PH_ADC_MAX);
      if (phTerputus) {
        hasLastGoodPh = false;
        phInvalidStreak = 99; // force immediate bypass
      }
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
    float ph = phTampilValid ? pH_value : 0.0f;
    bool phValid = phTampilValid && (ph >= 3.0f && ph <= 9.0f);
 
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
   Serial.print(getTimeStr());
   Serial.print(" T=");
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
   Serial.print(phSpread);
   Serial.print(" Kipas=");
  Serial.print(blowerOn ? 1 : 0);
   Serial.print(" Air=");
   Serial.print(waterOn ? 1 : 0);
   Serial.print(" PH=");
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
     doc["relay_kipas"] = blowerOn ? 1 : 0;
     doc["relay_air"] = waterOn ? 1 : 0;
     doc["relay_ph"] = phRelayOn ? 1 : 0;
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