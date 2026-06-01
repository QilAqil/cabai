/*
 * Sistem Monitoring & Kontrol Greenhouse CABAI RAWIT — ESP32
 * (Capsicum frutescens)
 *
 * Parameter optimal cabai rawit:
 *   - Kelembaban tanah : 50%-70%
 *   - pH tanah        : 6-7
 *   - Suhu udara      : 24-28°C (DHT22)
 *
 * Kontrol: FUZZY TAHANI (Mamdani), 3 jalur terpisah — selaras grafik.py & index.html
 * -------------------------------------------------------------------------
 * Tahapan per jalur:
 *   1. Fuzzifikasi input (μ) — kurva di grafik_fuzzy_input.png
 *   2. Aturan IF-THEN (lihat komentar tiap fuzzyXxxFrom...)
 *   3. Implikasi MIN, agregasi MAX
 *   4. Defuzzifikasi centroid → skor 0–1 (grafik_fuzzy_output.png)
 *   5. Aktuator ON jika skor >= RELAY_FUZZY_THRESHOLD (0,5)
 *
 * Jalur aktuator:
 *   - fuzzy_suhu  ← suhu DHT22     → servo paranet GPIO21 (kolom DB relay_paranet = flag 0/1)
 *   - fuzzy_soil  ← kelembaban %  → relay air GPIO26
 *   - fuzzy_ph    ← pH tanah       → relay pH GPIO27
 *
 * Sensor: DHT22 GPIO4 | soil AO GPIO35 | pH ADC GPIO34, DMS GPIO13
 * Cloud: MQTT topic pertanian/sensor (~10 s) | Supabase tabel pertanian (~30 s)
 * Daya: sensor & servo 3,3 V | modul relay 5 V | adaptor 12 V → expansion board
 *
 * Dashboard index.html: skor Tahani centroid; hijau jika >= 0,5 (sama ambang firmware).
 */

 #include <DHT.h>
 #include <ESP32Servo.h>
 #include <WiFi.h>
 #include <WiFiClientSecure.h>
 #include <PubSubClient.h>
 #include <HTTPClient.h>
 #include <ArduinoJson.h>
 
 // ==========================
 // Konfigurasi Sensor DHT22 (AM2302)
 // ==========================
 // Modul 3 pin (VCC, DATA, GND) dengan PCB — pull-up ~10 kΩ sudah di modul, tanpa resistor ekstra.
 // Suhu optimal 24-28°C; jalur fuzzy suhu → servo paranet (grafik.py: Rendah/Sedang/Tinggi °C).
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
 
 // LED indikator (biasanya LED built-in di GPIO2):
 // - Dipakai sebagai indikator tanah kering dan status pembacaan pH.
 const int LED_PIN    = 2;
 const int DRY_THRESHOLD_PERCENT = 50; // referensi kering (optimal cabai rawit 50-70%); fuzzy tanah tetap pakai kurva penuh
 
 // ============
 // Aktuator (hasil Fuzzy Tahani, ambang RELAY_FUZZY_THRESHOLD)
 // ============
 // - Paranet: servo PWM GPIO21, VCC 3,3 V — library "ESP32Servo".
 // - Air & pH: relay VCC 5 V, aktif-LOW (LOW = ON). Set RELAY_ACTIVE_LOW = false jika modul aktif-HIGH.
 const bool RELAY_ACTIVE_LOW = true;
 const int PARANET_SERVO_PIN = 21; // sinyal PWM servo paranet (sesuaikan wiring)
 // Sudut servo (0–180): kalibrasi mekanik roll paranet — boleh dibalik jika arah terbalik.
 const int PARANET_SERVO_ANGLE_OFF = 0;   // suhu rendah / tidak perlu naungan
 const int PARANET_SERVO_ANGLE_ON  = 90; // suhu tinggi / paranet diturunkan (ubah sesuai mekanik)
 const int RELAY_WATER_PIN   = 26; // relay pompa air
 const int RELAY_PH_PIN      = 27; // relay koreksi pH 
 Servo paranetServo;
 
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
 
 int   PH_ADC;          // nilai ADC mentah untuk pH
 float lastReading_pH;  // pH terakhir yang terbaca
 float pH_value;        // nilai pH saat ini
 
 // =============================================================================
 // LOGIKA FUZZY TAHANI (Mamdani, 3 jalur — selaras grafik.py)
 // =============================================================================
 // Tahapan: fuzzifikasi → aturan IF-THEN → implikasi MIN → agregasi MAX → centroid.
 // Fuzzifikasi: μ(x) seperti fuzzyLow / fuzzyMedium / fuzzyHigh (grafik.py).
 // Konsekuen linguistik pada domain keluaran [0,1]: Rendah / Sedang / Tinggi (intensitas).
 // Skor keluaran = titik berat (centroid) himpunan agregat; aktuator ON jika skor >= 0,5.
 //
 // Jalur 1: suhu → fuzzy_suhu  → servo paranet (GPIO21)
 // Jalur 2: tanah → fuzzy_soil → relay air (GPIO26)
 // Jalur 3: pH → fuzzy_ph → relay pH (GPIO27)
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
 
 static float outMuByKind(float y, int kind) {
   if (kind == OUT_RENDAH) return trapmf(y, 0.0f, 0.0f, 0.15f, 0.45f);
   if (kind == OUT_SEDANG) return trimf(y, 0.15f, 0.35f, 0.55f);
   return trapmf(y, 0.45f, 0.65f, 1.0f, 1.0f);
 }
 
 /** Defuzzifikasi centroid Mamdani/Tahani (implikasi MIN, agregasi MAX). */
 static float mamdaniTahaniCentroid(float mu1, int kind1, float mu2, int kind2, float mu3, int kind3) {
   const int STEPS = 20;
   float num = 0.0f, den = 0.0f;
   for (int i = 0; i <= STEPS; i++) {
     float y = (float)i / (float)STEPS;
     float agg = 0.0f;
     agg = fmaxfz(agg, fminfz(mu1, outMuByKind(y, kind1)));
     agg = fmaxfz(agg, fminfz(mu2, outMuByKind(y, kind2)));
     agg = fmaxfz(agg, fminfz(mu3, outMuByKind(y, kind3)));
     num += y * agg;
     den += agg;
   }
   return (den > 1e-6f) ? (num / den) : 0.0f;
 }
 
 // -----------------------------------------------------------------------------
 // JALUR 1 — SUHU (DHT22) → fuzzy_suhu → servo paranet
 // Input μ: trapmf/trimf suhu (grafik.py). Output: intensitas Rendah/Sedang/Tinggi [0,1].
 // IF suhu Rendah  THEN intensitas Rendah  | IF Sedang THEN Sedang | IF Tinggi THEN Tinggi
 // -----------------------------------------------------------------------------
 static float fuzzyParanetFromTemp(float tempC, bool tempValid) {
   if (!tempValid) return 0.0f;
   float muR = trapmf(tempC, 0.0f, 0.0f, 24.0f, 27.0f);
   float muS = trimf(tempC, 24.0f, 27.0f, 31.0f);
   float muT = trapmf(tempC, 27.0f, 31.0f, 45.0f, 45.0f);
   return mamdaniTahaniCentroid(muR, OUT_RENDAH, muS, OUT_SEDANG, muT, OUT_TINGGI);
 }
 
 // -----------------------------------------------------------------------------
 // JALUR 2 — TANAH (%) → fuzzy_soil → relay air
 // IF Kering THEN intensitas Tinggi (butuh siram) | IF Lembab/Basah THEN Rendah
 // -----------------------------------------------------------------------------
 static float fuzzyWaterFromSoil(int moisturePercent) {
   float x = (float)moisturePercent;
   float muK = trapmf(x, 0.0f, 0.0f, 40.0f, 50.0f);
   float muL = trapmf(x, 40.0f, 50.0f, 70.0f, 80.0f);
   float muB = trapmf(x, 70.0f, 80.0f, 100.0f, 100.0f);
   return mamdaniTahaniCentroid(muK, OUT_TINGGI, muL, OUT_RENDAH, muB, OUT_RENDAH);
 }
 
 // -----------------------------------------------------------------------------
 // JALUR 3 — pH → fuzzy_ph → relay koreksi larutan
 // IF Asam THEN intensitas Tinggi (koreksi) | IF Netral/Basa THEN Rendah
 // -----------------------------------------------------------------------------
 static float fuzzyPhCorrectionFromPh(float ph, bool phValid) {
   if (!phValid) return 0.0f;
   float muA = trapmf(ph, 3.0f, 3.0f, 5.0f, 6.0f);
   float muN = trapmf(ph, 5.5f, 6.0f, 7.0f, 7.5f);
   float muB = trapmf(ph, 7.0f, 7.5f, 9.0f, 9.0f);
   return mamdaniTahaniCentroid(muA, OUT_TINGGI, muN, OUT_RENDAH, muB, OUT_RENDAH);
 }
 
 /** Servo paranet: OFF/ON dari fuzzy_suhu >= ambang (bukan sudut kontinu). */
 static void paranetServoApply(bool deployed) {
   int a = deployed ? PARANET_SERVO_ANGLE_ON : PARANET_SERVO_ANGLE_OFF;
   if (a < 0) a = 0;
   if (a > 180) a = 180;
   paranetServo.write(a);
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
   float ph, float fuzzy_paranet, float fuzzy_soil, float fuzzy_ph,
   bool paranet_on, bool relay_air, bool relay_ph
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
 
   // Supabase: fuzzy_* = skor centroid Tahani; relay_paranet = flag servo paranet.
   String body = "{";
   body += "\"temperature\":" + String(temperature, 2) + ",";
   body += "\"humidity\":" + String(humidity, 2) + ",";
   body += "\"soil\":" + String(soil) + ",";
   body += "\"ph\":" + String(ph, 1) + ",";
   body += "\"fuzzy_suhu\":" + String(fuzzy_paranet, 2) + ",";
   body += "\"fuzzy_soil\":" + String(fuzzy_soil, 2) + ",";
   body += "\"fuzzy_ph\":" + String(fuzzy_ph, 2) + ",";
   // relay_paranet = flag 0/1 servo paranet (nama kolom di DB tetap relay_paranet / float4).
   body += "\"relay_paranet\":" + String(paranet_on ? 1 : 0) + ",";
   body += "\"relay_air\":" + String(relay_air ? 1 : 0) + ",";
   body += "\"relay_dolomit\":" + String(relay_ph ? 1 : 0);
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
 
   // Servo paranet + relay air / pH
   paranetServo.attach(PARANET_SERVO_PIN);
   paranetServo.write(PARANET_SERVO_ANGLE_OFF);
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
 
   // Baca nilai analog (0 - 4095 untuk ESP32)
   int sensorValue = analogRead(SOIL_PIN);
 
   // Konversi ke persen kelembaban (0–100%)
   // Catatan: pada banyak sensor, nilai ADC KECIL = tanah BASAH, BESAR = tanah KERING
   int moisturePercent = map(sensorValue, DRY_VALUE, WET_VALUE, 0, 100);
   moisturePercent = constrain(moisturePercent, 0, 100);
 
   // Baca DHT22 (min. ~2 s antar pembacaan disarankan; loop sudah jeda panjang karena pH)
   float h = dht.readHumidity();
   float t = dht.readTemperature(); // default Celcius
   bool dhtOk = !(isnan(h) || isnan(t));
 
   if (!dhtOk) {
     Serial.println("DHT22 error");
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
 
   // ===== Defuzzifikasi Tahani (centroid 0–1), aktuator ON jika >= RELAY_FUZZY_THRESHOLD =====
   float ph = lastReading_pH;
   bool phValid = (ph >= 3.0f && ph <= 9.0f);
 
   float scoreParanet = fuzzyParanetFromTemp(t, dhtOk);       // → fuzzy_suhu
   float scoreSoil = fuzzyWaterFromSoil(moisturePercent);    // → fuzzy_soil
   float scorePh = fuzzyPhCorrectionFromPh(ph, phValid);    // → fuzzy_ph
 
   bool paranetOn = dhtOk && (scoreParanet >= RELAY_FUZZY_THRESHOLD);
   bool waterOn = (scoreSoil >= RELAY_FUZZY_THRESHOLD);
   bool phRelayOn = phValid && (scorePh >= RELAY_FUZZY_THRESHOLD);
 
   paranetServoApply(paranetOn);
   relayWrite(RELAY_WATER_PIN, waterOn);
   relayWrite(RELAY_PH_PIN, phRelayOn);
 
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
   Serial.print(" P=");
   Serial.print(paranetOn ? 1 : 0);
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
     doc["ph"] = phRounded;
     // Skor centroid Tahani 0–1 (index.html, ambang tampilan 0,5)
     doc["fuzzy_suhu"] = scoreParanet;
     doc["fuzzy_soil"] = scoreSoil;
     doc["fuzzy_ph"] = scorePh;
     doc["relay_paranet"] = paranetOn ? 1 : 0;
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
     supabaseInsert(t, h, moisturePercent, phRounded, scoreParanet, scoreSoil, scorePh,
                    paranetOn, waterOn, phRelayOn);
   }
 
   waitWithMqtt(3UL * 1000UL); // jeda sebelum pembacaan berikutnya (tetap jaga MQTT)
 }