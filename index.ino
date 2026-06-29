/*********
 * Sistem IoT Pertanian Cerdas Cabai Rawit
 * Compile using Arduino IDE 2.x
 * Board: ESP32 Dev Module (ESP32-30P)
 *
 * Sensor   : DHT22 (GPIO4), Soil Moisture (GPIO35), pH+DMS (GPIO34+GPIO13)
 * Aktuator : Relay Kipas (GPIO26), Relay Pompa Air (GPIO27), Relay Pompa pH (GPIO25)
 * Koneksi  : WiFi → MQTT (EMQX TLS 8883) + Supabase REST API
 * Logika   : Fuzzy Tahani — fungsi eksplisit per himpunan
 *********/

// ════════════════════════════════════════════════════════════════
//  LIBRARY
// ════════════════════════════════════════════════════════════════
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <time.h>         // NTP — built-in ESP32, tidak perlu install library

// ════════════════════════════════════════════════════════════════
//  KONFIGURASI WiFi
// ════════════════════════════════════════════════════════════════
const char* WIFI_SSID = "UPT-LAB-KOM";
const char* WIFI_PASS = "uptlab12";

// ════════════════════════════════════════════════════════════════
//  KONFIGURASI MQTT (EMQX Cloud, TLS port 8883)
// ════════════════════════════════════════════════════════════════
const char* MQTT_SERVER = "n01d3130.ala.asia-southeast1.emqxsl.com";
const int   MQTT_PORT   = 8883;
const char* MQTT_USER   = "pertanian";
const char* MQTT_PASS   = "pertanian12";
const char* TOPIC_PUB   = "pertanian/sensor";
const char* TOPIC_SUB   = "pertanian/kontrol";  // topic perintah manual dari dashboard

// ════════════════════════════════════════════════════════════════
//  KONFIGURASI SUPABASE
// ════════════════════════════════════════════════════════════════
#define SUPABASE_URL  "https://sptomqebtvclfebaktof.supabase.co"
#define SUPABASE_KEY  "sb_publishable_jqEF4hY0nK0Bu0BkKK2ayQ_eW_8fh_u"
#define TABLE_NAME    "pertanian"

// ════════════════════════════════════════════════════════════════
//  KONFIGURASI PIN
// ════════════════════════════════════════════════════════════════
#define DHTPIN   4
#define DHTTYPE  DHT22

const int SOIL_PIN      = 35;
const int PH_ADC_PIN    = 34;
const int DMS_PIN       = 13;
const int LED_PIN       = 2;

const int RELAY_KIPAS     = 25;
const int RELAY_POMPA_AIR = 27;
const int RELAY_POMPA_PH  = 26;

// ════════════════════════════════════════════════════════════════
//  KALIBRASI & INTERVAL
// ════════════════════════════════════════════════════════════════
// ════════════════════════════════════════════════════════════════
//  KALIBRASI SOIL MOISTURE (ADC 12-bit, 0–4095)
//  Cara kalibrasi:
//    1. Cabut sensor dari tanah, di udara terbuka → catat ADC → SOIL_ADC_KERING
//    2. Celupkan ujung sensor ke dalam air        → catat ADC → SOIL_ADC_BASAH
//  Nilai di bawah estimasi awal — WAJIB disesuaikan dengan sensor Anda
// ════════════════════════════════════════════════════════════════
const int SOIL_ADC_KERING = 3800;   // ADC saat kering di udara  → 0%
const int SOIL_ADC_BASAH  =  800;   // ADC saat basah dalam air  → 100%

const unsigned long INTERVAL_BACA   = 15000UL;  // baca sensor + kirim MQTT tiap 15 detik
const unsigned long INTERVAL_DB     = 60000UL;  // kirim Supabase tiap 60 detik (1 menit)
const unsigned long INTERVAL_PH_ON  =  5000UL;  // DMS aktif 5 detik sebelum baca pH
const unsigned long INTERVAL_PH_OFF =  2000UL;  // jeda 2 detik setelah baca pH

// ════════════════════════════════════════════════════════════════
//  OBJEK
// ════════════════════════════════════════════════════════════════
DHT              dht(DHTPIN, DHTTYPE);
WiFiClientSecure tlsClient;       // khusus MQTT
PubSubClient     mqttClient(tlsClient);

// ════════════════════════════════════════════════════════════════
//  VARIABEL GLOBAL
// ════════════════════════════════════════════════════════════════
float g_suhu       = 0.0f;
float g_kelembaban = 0.0f;
float g_soil       = 0.0f;
float g_pH         = 0.0f;
int   g_adcPH      = 0;
int   g_adcSoil    = 0;

bool relay_kipas_state     = false;
bool relay_pompa_air_state = false;
bool relay_pompa_ph_state  = false;

// Mode manual override dari dashboard
bool   manual_mode      = false;
bool   manual_kipas     = false;
bool   manual_pompa_air = false;
bool   manual_pompa_ph  = false;

unsigned long lastBacaMillis = 0;
unsigned long lastDBMillis   = 0;   // timer pengiriman ke Supabase

// ════════════════════════════════════════════════════════════════
//  STRUCT FUZZY OUTPUT
//  Dideklarasikan di sini agar bisa dipakai oleh semua fungsi
//  di bawahnya (publishMQTT, insertSupabase, inferensiFuzzy)
// ════════════════════════════════════════════════════════════════
struct FuzzyOutput {
  bool  kipas;
  bool  pompa_air;
  bool  pompa_ph;
  float mu_kipas;      // → fuzzy_suhu di Supabase
  float mu_pompa_air;  // → fuzzy_soil di Supabase
  float mu_pompa_ph;   // → fuzzy_ph   di Supabase
  const char* status_suhu;
  const char* status_tanah;
  const char* status_ph;
};

// ════════════════════════════════════════════════════════════════
//  FUNGSI KEANGGOTAAN — SUHU UDARA (°C)
// ════════════════════════════════════════════════════════════════

// fuzzyTempRendah: trapmf(0, 0, 24, 27)
//   Jika suhu <= 24  → sepenuhnya Rendah (= 1)
//   Jika suhu 24–27  → menurun linier: (27 - x) / 3
//   Jika suhu >= 27  → bukan Rendah (= 0)
static float fuzzyTempRendah(float x) {
  if (x <= 24.0f)                    return 1.0f;
  else if (x > 24.0f && x < 27.0f)  return (27.0f - x) / 3.0f;
  else                               return 0.0f;
}

// fuzzyTempSedang: trimf(24, 27, 31)
//   Jika suhu <= 24 atau >= 31  → bukan Sedang (= 0)
//   Jika suhu 24–27  → meningkat: (x - 24) / 3
//   Jika suhu 27–31  → menurun:   (31 - x) / 4
static float fuzzyTempSedang(float x) {
  if (x <= 24.0f || x >= 31.0f)     return 0.0f;
  else if (x > 24.0f && x <= 27.0f) return (x - 24.0f) / 3.0f;
  else                               return (31.0f - x) / 4.0f;
}

// fuzzyTempTinggi: trapmf(27, 31, 45, 45)
//   Jika suhu <= 27  → bukan Tinggi (= 0)
//   Jika suhu 27–31  → meningkat: (x - 27) / 4
//   Jika suhu >= 31  → sepenuhnya Tinggi (= 1)
static float fuzzyTempTinggi(float x) {
  if (x <= 27.0f)                    return 0.0f;
  else if (x > 27.0f && x < 31.0f)  return (x - 27.0f) / 4.0f;
  else                               return 1.0f;
}

// fuzzyTempStatus: status suhu berdasarkan derajat keanggotaan tertinggi
static const char* fuzzyTempStatus(float x) {
  float r = fuzzyTempRendah(x);
  float s = fuzzyTempSedang(x);
  float t = fuzzyTempTinggi(x);
  if (r >= s && r >= t) return "Rendah";
  else if (s >= t)      return "Sedang";
  else                  return "Tinggi";
}

// ════════════════════════════════════════════════════════════════
//  FUNGSI KEANGGOTAAN — KELEMBABAN TANAH (%)
// ════════════════════════════════════════════════════════════════

// fuzzySoilKering: trapmf(0, 0, 40, 50)
//   Jika soil <= 40  → sepenuhnya Kering (= 1)
//   Jika soil 40–50  → menurun: (50 - x) / 10
//   Jika soil >= 50  → bukan Kering (= 0)
static float fuzzySoilKering(float x) {
  if (x <= 40.0f)                    return 1.0f;
  else if (x > 40.0f && x < 50.0f)  return (50.0f - x) / 10.0f;
  else                               return 0.0f;
}

// fuzzySoilLembab: trapmf(40, 50, 70, 80)
//   Jika soil <= 40 atau >= 80   → bukan Lembab (= 0)
//   Jika soil 40–50  → meningkat: (x - 40) / 10
//   Jika soil 50–70  → sepenuhnya Lembab (= 1)
//   Jika soil 70–80  → menurun:   (80 - x) / 10
static float fuzzySoilLembab(float x) {
  if (x <= 40.0f || x >= 80.0f)      return 0.0f;
  else if (x > 40.0f && x < 50.0f)   return (x - 40.0f) / 10.0f;
  else if (x >= 50.0f && x <= 70.0f) return 1.0f;
  else                                return (80.0f - x) / 10.0f;
}

// fuzzySoilBasah: trapmf(70, 80, 100, 100)
//   Jika soil <= 70  → bukan Basah (= 0)
//   Jika soil 70–80  → meningkat: (x - 70) / 10
//   Jika soil >= 80  → sepenuhnya Basah (= 1)
static float fuzzySoilBasah(float x) {
  if (x <= 70.0f)                    return 0.0f;
  else if (x > 70.0f && x < 80.0f)  return (x - 70.0f) / 10.0f;
  else                               return 1.0f;
}

// fuzzySoilStatus: status tanah berdasarkan derajat keanggotaan tertinggi
static const char* fuzzySoilStatus(float x) {
  float k = fuzzySoilKering(x);
  float l = fuzzySoilLembab(x);
  float b = fuzzySoilBasah(x);
  if (k >= l && k >= b) return "Kering";
  else if (l >= b)      return "Lembab";
  else                  return "Basah";
}

// ════════════════════════════════════════════════════════════════
//  FUNGSI KEANGGOTAAN — pH TANAH
// ════════════════════════════════════════════════════════════════

// fuzzyPhAsam: trapmf(3, 3, 5, 6)
//   Jika pH <= 5  → sepenuhnya Asam (= 1)
//   Jika pH 5–6   → menurun: (6 - x) / 1
//   Jika pH >= 6  → bukan Asam (= 0)
static float fuzzyPhAsam(float x) {
  if (x <= 5.0f)                   return 1.0f;
  else if (x > 5.0f && x < 6.0f)  return (6.0f - x) / 1.0f;
  else                             return 0.0f;
}

// fuzzyPhNormal: trapmf(5.5, 6, 7, 7.5)
//   Jika pH <= 5.5 atau >= 7.5  → bukan Normal (= 0)
//   Jika pH 5.5–6   → meningkat: (x - 5.5) / 0.5
//   Jika pH 6–7     → sepenuhnya Normal (= 1)
//   Jika pH 7–7.5   → menurun:   (7.5 - x) / 0.5
static float fuzzyPhNormal(float x) {
  if (x <= 5.5f || x >= 7.5f)       return 0.0f;
  else if (x > 5.5f && x < 6.0f)    return (x - 5.5f) / 0.5f;
  else if (x >= 6.0f && x <= 7.0f)  return 1.0f;
  else if (x > 7.0f && x < 7.5f)    return (7.5f - x) / 0.5f;
  else                               return 0.0f;
}

// fuzzyPhBasa: trapmf(7, 7.5, 9, 9)
//   Jika pH <= 7    → bukan Basa (= 0)
//   Jika pH 7–7.5   → meningkat: (x - 7) / 0.5
//   Jika pH >= 7.5  → sepenuhnya Basa (= 1)
static float fuzzyPhBasa(float x) {
  if (x <= 7.0f)                   return 0.0f;
  else if (x > 7.0f && x < 7.5f)  return (x - 7.0f) / 0.5f;
  else                             return 1.0f;
}

// fuzzyPhStatus: status pH berdasarkan derajat keanggotaan tertinggi
static const char* fuzzyPhStatus(float x) {
  float a = fuzzyPhAsam(x);
  float n = fuzzyPhNormal(x);
  float b = fuzzyPhBasa(x);
  if (a >= n && a >= b) return "Asam";
  else if (n >= b)      return "Normal";
  else                  return "Basa";
}

// ════════════════════════════════════════════════════════════════
//  INFERENSI FUZZY TAHANI
//  Operator: AND = min, OR = max
//  R1: IF suhu Tinggi                      → KIPAS ON
//  R2: IF tanah Kering                     → POMPA AIR ON
//  R3: IF tanah Lembab AND suhu Tinggi     → POMPA AIR ON
//  R4: IF pH Asam                          → POMPA pH ON
//  R5: IF pH Basa                          → POMPA pH ON
// ════════════════════════════════════════════════════════════════
FuzzyOutput inferensiFuzzy(float suhu, float soil, float pH) {
  FuzzyOutput out;

  // — Hitung derajat keanggotaan yang dipakai di rule
  float mu_st = fuzzyTempTinggi(suhu);
  float mu_tk = fuzzySoilKering(soil);
  float mu_tl = fuzzySoilLembab(soil);
  float mu_pa = fuzzyPhAsam(pH);
  float mu_pb = fuzzyPhBasa(pH);

  // — Status label (argmax via fungsi status)
  out.status_suhu  = fuzzyTempStatus(suhu);
  out.status_tanah = fuzzySoilStatus(soil);
  out.status_ph    = fuzzyPhStatus(pH);

  // — Rule Kipas
  out.mu_kipas = mu_st;
  out.kipas    = (mu_st > 0.5f);

  // — Rule Pompa Air
  float r2 = mu_tk;
  float r3 = min(mu_tl, mu_st);      // AND = min
  out.mu_pompa_air = max(r2, r3);     // OR  = max
  out.pompa_air    = (out.mu_pompa_air > 0.4f);

  // — Rule Pompa pH
  // Hanya pH Asam yang mengaktifkan pompa pH
  // pH Basa → mu = 0 → relay OFF (tidak ada koreksi)
  out.mu_pompa_ph = mu_pa;              // hanya gunakan derajat keanggotaan Asam
  out.pompa_ph    = (mu_pa > 0.4f);

  return out;
}

// ════════════════════════════════════════════════════════════════
//  SENSOR pH — PIPELINE FILTER 11 TAHAP
//  Tahap 1 : ADC 12-bit + Attenuation 11dB
//  Tahap 2 : 150 sampel × 5ms
//  Tahap 3-5: Sort → Trimmed mean 25% → Median
//  Tahap 6 : Gabung weighted trimmed 60% + median 40%
//  Tahap 7 : Piecewise kalibrasi 3 titik
//  Tahap 8 : Kompensasi suhu Nernst
//  Tahap 9 : Noise gate ±0.8 pH
//  Tahap 10: Micro-buffer 3 + Weighted MA 12 + EMA 0.08
//  Tahap 11: Rate limiter 0.01/siklus + Hysteresis 0.03
//  Deteksi probe: konfirmasi 5 siklus berturut-turut
// ════════════════════════════════════════════════════════════════

struct PhCalPoint { int adc; float ph; };
const PhCalPoint PH_CAL[3] = {
  { 1062, 4.01f },   // buffer pH 4.01 — ganti ADC dari Serial Monitor
  {  804, 6.86f },   // buffer pH 6.86 — ganti ADC dari Serial Monitor
  {  600, 9.18f },   // buffer pH 9.18 — ganti ADC dari Serial Monitor
};

const int   PH_SAMPLES      = 150;
const int   PH_TRIM_PCT     = 25;
const int   PH_MA_SIZE      = 20;     // naik dari 12 → 20 siklus (~5 menit)
const int   PH_MICRO_SIZE   = 5;      // naik dari 3 → 5 siklus
const float PH_EMA_ALPHA    = 0.05f;  // turun dari 0.08 → 0.05 (lebih halus)
const float PH_NOISE_GATE   = 0.80f;
const float PH_RATE_LIMIT   = 0.005f; // turun dari 0.01 → 0.005 per siklus
const float PH_HYSTERESIS   = 0.05f;  // naik dari 0.03 → 0.05
const float PH_TEMP_COEF    = 0.003f;
const float PH_TEMP_REF     = 25.0f;
const int   PH_ADC_BATAS    = 1080;
const int   PH_NOTANCAP_CNT = 5;

static float phMicro[5]       = {0,0,0,0,0};  // micro-buffer 5 siklus
static int   phMicro_idx      = 0;
static bool  phMicro_full     = false;
static float phMA[20]         = {0};           // MA 20 siklus
static int   phMA_idx         = 0;
static bool  phMA_full        = false;
static float phEMA            = 0.0f;
static float phLast           = 0.0f;
static float phOutput         = 0.0f;
static int   phNotTancapCount = 0;
// Warmup: abaikan N pembacaan pertama setelah boot agar elektroda stabil
static int   phWarmupCount    = 0;
const  int   PH_WARMUP_N      = 5;   // abaikan 5 pembacaan pertama (~75 detik)

static void sortArray(int* arr, int n) {
  for (int i = 1; i < n; i++) {
    int key = arr[i], j = i - 1;
    while (j >= 0 && arr[j] > key) { arr[j+1] = arr[j]; j--; }
    arr[j+1] = key;
  }
}

static float adcToPH(int adc) {
  int idx[3] = {0, 1, 2};
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < 2-i; j++)
      if (PH_CAL[idx[j]].adc > PH_CAL[idx[j+1]].adc) {
        int t = idx[j]; idx[j] = idx[j+1]; idx[j+1] = t;
      }
  int   a0=PH_CAL[idx[0]].adc, a1=PH_CAL[idx[1]].adc, a2=PH_CAL[idx[2]].adc;
  float p0=PH_CAL[idx[0]].ph,  p1=PH_CAL[idx[1]].ph,  p2=PH_CAL[idx[2]].ph;
  if      (adc <= a0) { float m=(p1-p0)/(float)(a1-a0); return p0+m*(adc-a0); }
  else if (adc <= a1) { float r=(float)(adc-a0)/(float)(a1-a0); return p0+r*(p1-p0); }
  else if (adc <= a2) { float r=(float)(adc-a1)/(float)(a2-a1); return p1+r*(p2-p1); }
  else                { float m=(p2-p1)/(float)(a2-a1); return p1+m*(adc-a1); }
}

static float kompensasiSuhu(float ph, float suhu) {
  return ph - (PH_TEMP_COEF * (suhu - PH_TEMP_REF) * (ph - 7.0f));
}

float bacaPH() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  digitalWrite(DMS_PIN, LOW);
  digitalWrite(LED_PIN, HIGH);
  // Tunggu DMS stabil — sambil tetap proses MQTT setiap 500ms
  unsigned long dmsStart = millis();
  while (millis() - dmsStart < INTERVAL_PH_ON) {
    mqttClient.loop();
    delay(500);
  }

  int s[PH_SAMPLES];
  for (int i = 0; i < PH_SAMPLES; i++) {
    s[i] = analogRead(PH_ADC_PIN);
    delay(5);
    // Proses pesan MQTT setiap 10 sampel agar tidak tertunda
    if (i % 10 == 0) mqttClient.loop();
  }

  digitalWrite(DMS_PIN, HIGH);
  digitalWrite(LED_PIN, LOW);
  delay(INTERVAL_PH_OFF);

  // Sort
  sortArray(s, PH_SAMPLES);

  // Median
  int adcMed = (s[74] + s[75]) / 2;

  // Trimmed mean 25%
  int  trimN = PH_SAMPLES * PH_TRIM_PCT / 100;
  long sumT  = 0; int cntT = 0;
  for (int i = trimN; i < PH_SAMPLES-trimN; i++) { sumT += s[i]; cntT++; }
  int adcTrim = (int)(sumT / cntT);

  // Weighted combine: trimmed 60% + median 40%
  int adcFinal = (int)(adcTrim * 0.60f + adcMed * 0.40f);
  g_adcPH = adcFinal;

  // Deteksi probe tidak terpasang (konfirmasi 5x)
  if (adcFinal >= PH_ADC_BATAS) {
    phNotTancapCount++;
    if (phNotTancapCount >= PH_NOTANCAP_CNT) {
      Serial.printf("[pH] Tidak terpasang (ADC=%d, %d/%d)\n",
        adcFinal, phNotTancapCount, PH_NOTANCAP_CNT);
      phLast = phOutput = phEMA = 0.0f;
      phMicro_idx = phMA_idx = 0;
      phMicro_full = phMA_full = false;
      return 0.0f;
    }
    Serial.printf("[pH] ADC=%d >= %d (%d/%d), pakai terakhir\n",
      adcFinal, PH_ADC_BATAS, phNotTancapCount, PH_NOTANCAP_CNT);
    return phLast;
  }
  phNotTancapCount = 0;

  // Konversi + kompensasi suhu
  float phRaw  = adcToPH(adcFinal);
  if (phRaw < 0.0f || phRaw > 14.0f) return phLast;
  float phComp = kompensasiSuhu(phRaw, g_suhu);

  // Noise gate
  if (phLast > 0.0f && fabsf(phComp - phLast) > PH_NOISE_GATE) {
    Serial.printf("[pH] Noise gate: delta=%.3f\n", fabsf(phComp-phLast));
    return phLast;
  }

  // Micro-buffer 3 siklus
  phMicro[phMicro_idx] = phComp;
  phMicro_idx = (phMicro_idx + 1) % PH_MICRO_SIZE;
  if (phMicro_idx == 0) phMicro_full = true;
  int   mcnt = phMicro_full ? PH_MICRO_SIZE : phMicro_idx;
  float phMV = 0.0f;
  for (int i = 0; i < mcnt; i++) phMV += phMicro[i];
  phMV /= mcnt;

  // Weighted moving average 12 siklus (bobot linier, terbaru lebih besar)
  phMA[phMA_idx] = phMV;
  phMA_idx = (phMA_idx + 1) % PH_MA_SIZE;
  if (phMA_idx == 0) phMA_full = true;
  int   macnt  = phMA_full ? PH_MA_SIZE : phMA_idx;
  float phMAv  = 0.0f, wTot = 0.0f;
  for (int i = 0; i < macnt; i++) {
    int   wi = (phMA_idx - macnt + i + PH_MA_SIZE) % PH_MA_SIZE;
    float w  = 1.0f + (float)i;
    phMAv   += phMA[wi] * w;
    wTot    += w;
  }
  phMAv /= wTot;

  // EMA
  phEMA = (phEMA == 0.0f) ? phMAv
        : PH_EMA_ALPHA * phMAv + (1.0f - PH_EMA_ALPHA) * phEMA;

  // Rate limiter + hysteresis
  if (phOutput == 0.0f) {
    phOutput = phEMA;
  } else if (fabsf(phEMA - phOutput) > PH_HYSTERESIS) {
    float diff = phEMA - phOutput;
    phOutput  += (fabsf(diff) > PH_RATE_LIMIT)
      ? ((diff > 0) ? PH_RATE_LIMIT : -PH_RATE_LIMIT)
      : diff;
  }

  phLast = phOutput;

  Serial.printf("[pH] ADCtrim=%d ADCmed=%d ADCfinal=%d "
                "phRaw=%.3f phComp=%.3f phMicro=%.3f phMA=%.3f phEMA=%.3f phOut=%.3f\n",
    adcTrim, adcMed, adcFinal,
    phRaw, phComp, phMV, phMAv, phEMA, phOutput);

  return phOutput;
}

// ════════════════════════════════════════════════════════════════
//  BACA SENSOR SOIL MOISTURE
//  ADC 12-bit → dipetakan ke persentase 0–100%
// ════════════════════════════════════════════════════════════════
float bacaSoil() {
  analogReadResolution(12);
  int raw = analogRead(SOIL_PIN);
  g_adcSoil = raw;
  float pct = map(raw, SOIL_ADC_KERING, SOIL_ADC_BASAH, 0, 100);
  return constrain(pct, 0.0f, 100.0f);
}

// ════════════════════════════════════════════════════════════════
//  KONTROL RELAY  (aktif LOW)
// ════════════════════════════════════════════════════════════════
void setRelay(int pin, bool aktif) {
  digitalWrite(pin, aktif ? LOW : HIGH);
}

void terapkanAktuator(bool kipas, bool pompa_air, bool pompa_ph) {
  relay_kipas_state     = kipas;
  relay_pompa_air_state = pompa_air;
  relay_pompa_ph_state  = pompa_ph;
  setRelay(RELAY_KIPAS,     kipas);
  setRelay(RELAY_POMPA_AIR, pompa_air);
  setRelay(RELAY_POMPA_PH,  pompa_ph);
}

// ════════════════════════════════════════════════════════════════
//  PUBLISH MQTT
// ════════════════════════════════════════════════════════════════
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
  // — Info koneksi WiFi
  doc["wifi_ssid"]    = WiFi.SSID();
  doc["wifi_ip"]      = WiFi.localIP().toString();
  doc["wifi_rssi"]    = WiFi.RSSI();
  doc["wifi_status"]  = (WiFi.status() == WL_CONNECTED) ? "Terhubung" : "Terputus";

  char buf[640];
  serializeJson(doc, buf);
  mqttClient.setBufferSize(640);
  mqttClient.publish(TOPIC_PUB, buf, true);
}

// ════════════════════════════════════════════════════════════════
//  INSERT SUPABASE  — tambah baris baru setiap pengiriman
//  Menggunakan WiFiClientSecure + HTTPClient LOKAL (tidak berbagi
//  dengan tlsClient MQTT) agar tidak bentrok dan error -5 hilang.
// ════════════════════════════════════════════════════════════════
void insertSupabase(const FuzzyOutput& fo) {
  // — Bangun JSON body
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
  // — Waktu WIB dari NTP (override updated_at Supabase agar sesuai zona waktu)
  String iso = getWaktuISO();
  if (iso.length() > 0) doc["updated_at"] = iso;

  char body[448];
  serializeJson(doc, body);

  // — Buat objek TLS & HTTP lokal (bukan global tlsClient MQTT)
  WiFiClientSecure sbClient;
  sbClient.setInsecure();           // skip verifikasi CA

  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/" + TABLE_NAME;

  if (!http.begin(sbClient, url)) {
    Serial.println("[Supabase] http.begin() gagal");    return;
  }

  http.setTimeout(10000);           // timeout 10 detik
  http.addHeader("Content-Type",  "application/json");
  http.addHeader("apikey",        SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("Prefer",        "return=minimal");

  int code = http.POST(body);
  if (code == 201) {
    // Insert OK
  } else {
    Serial.println("[Supabase] Error " + String(code) + ": " + http.getString());
  }
  http.end();
  sbClient.stop();                  // tutup koneksi TLS dengan bersih
}

// ════════════════════════════════════════════════════════════════
//  CALLBACK MQTT — terima perintah override manual dari dashboard
//  Contoh payload:
//    {"manual":true,"kipas":true,"pompa_air":false,"pompa_ph":false}
//    {"manual":false}  ← kembali otomatis
// ════════════════════════════════════════════════════════════════
// ════════════════════════════════════════════════════════════════
//  NTP — Sinkronisasi waktu via internet (WIB = UTC+7)
// ════════════════════════════════════════════════════════════════
#define NTP_SERVER  "pool.ntp.org"
#define NTP_OFFSET  25200   // UTC+7 dalam detik (7 * 3600)
#define NTP_SYNC_MS 60000   // sync ulang tiap 60 detik

void ntpSync() {
  configTime(NTP_OFFSET, 0, NTP_SERVER);
  // Tunggu hingga waktu valid (tahun > 2020)
  struct tm ti;
  unsigned long t = millis();
  while (!getLocalTime(&ti) && millis() - t < 5000) delay(200);
  if (ti.tm_year + 1900 > 2020) {
    Serial.printf("[NTP]  Waktu sync : %02d/%02d/%04d %02d:%02d:%02d WIB\n",
      ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900,
      ti.tm_hour, ti.tm_min, ti.tm_sec);
  } else {
    Serial.println("[NTP]  Gagal sync waktu");
  }
}

// Kembalikan string waktu "HH:MM:SS" dari NTP
String getWaktu() {
  struct tm ti;
  if (!getLocalTime(&ti)) return "--:--:--";
  char buf[12];
  strftime(buf, sizeof(buf), "%H:%M:%S", &ti);
  return String(buf);
}

// Kembalikan string datetime lengkap untuk Supabase ISO8601
String getWaktuISO() {
  struct tm ti;
  if (!getLocalTime(&ti)) return "";
  char buf[30];
  // Format: 2026-06-25T13:25:32+07:00
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S+07:00", &ti);
  return String(buf);
}

// ════════════════════════════════════════════════════════════════
//  KONEKSI WiFi
// ════════════════════════════════════════════════════════════════
void koneksiWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) {
    delay(500);
  }
  if (WiFi.status() != WL_CONNECTED) ESP.restart();
}

// ════════════════════════════════════════════════════════════════
//  MQTT CALLBACK — terima perintah manual dari dashboard
//  Payload JSON: {"manual":true,"kipas":true,"pompa_air":false,"pompa_ph":false}
//  Kembali otomatis: {"manual":false}
// ════════════════════════════════════════════════════════════════
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<200> doc;
  if (deserializeJson(doc, payload, length)) return;

  if (doc.containsKey("manual")) manual_mode = doc["manual"].as<bool>();

  if (manual_mode) {
    if (doc.containsKey("kipas"))     manual_kipas     = doc["kipas"].as<bool>();
    if (doc.containsKey("pompa_air")) manual_pompa_air = doc["pompa_air"].as<bool>();
    if (doc.containsKey("pompa_ph"))  manual_pompa_ph  = doc["pompa_ph"].as<bool>();
    // Terapkan relay LANGSUNG — tidak menunggu siklus berikutnya
    terapkanAktuator(manual_kipas, manual_pompa_air, manual_pompa_ph);
    Serial.printf("[Manual] Kipas:%s Air:%s pH:%s\n",
      manual_kipas     ? "ON" : "OFF",
      manual_pompa_air ? "ON" : "OFF",
      manual_pompa_ph  ? "ON" : "OFF");
    // Kirim feedback ke dashboard segera setelah relay berubah
    // Gunakan nilai fuzzy terakhir sebagai payload
    FuzzyOutput fo = inferensiFuzzy(g_suhu, g_soil, g_pH);
    publishMQTT(fo);
  } else {
    Serial.println("[Manual] Mode otomatis aktif");
  }
}

// ════════════════════════════════════════════════════════════════
//  KONEKSI MQTT
// ════════════════════════════════════════════════════════════════
void koneksiMQTT() {
  tlsClient.setInsecure();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(640);

  String clientId = "ESP32-Cabai-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  Serial.print("[MQTT] Menghubungkan ke broker...");
  while (!mqttClient.connected()) {
    if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
      Serial.println(" OK");
      Serial.println("[MQTT] Topic pub : " + String(TOPIC_PUB));
      Serial.println("[MQTT] Topic sub : " + String(TOPIC_SUB));
      mqttClient.subscribe(TOPIC_SUB);
    } else {
      Serial.print(".");
      delay(3000);
    }
  }
}

// ════════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n========================================");
  Serial.println("  Sistem IoT Pertanian Cerdas Cabai");
  Serial.println("========================================");

  pinMode(DMS_PIN,         OUTPUT);
  pinMode(LED_PIN,         OUTPUT);
  pinMode(RELAY_KIPAS,     OUTPUT);
  pinMode(RELAY_POMPA_AIR, OUTPUT);
  pinMode(RELAY_POMPA_PH,  OUTPUT);

  setRelay(RELAY_KIPAS,     false);
  setRelay(RELAY_POMPA_AIR, false);
  setRelay(RELAY_POMPA_PH,  false);
  digitalWrite(DMS_PIN, HIGH);

  dht.begin();

  // — WiFi
  Serial.print("[WiFi] Menghubungkan ke " + String(WIFI_SSID) + "...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) {
    delay(500); Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" OK");
    Serial.println("[WiFi] IP Address : " + WiFi.localIP().toString());
    Serial.println("[WiFi] RSSI       : " + String(WiFi.RSSI()) + " dBm");
    ntpSync();   // sync waktu setelah WiFi terhubung
  } else {
    Serial.println(" GAGAL! Restart...");
    ESP.restart();
  }

  koneksiMQTT();

  Serial.println("[INFO] Interval baca  : " + String(INTERVAL_BACA / 1000) + " detik (~" + String((INTERVAL_BACA + INTERVAL_PH_ON + INTERVAL_PH_OFF) / 1000) + "s/siklus)");
  Serial.println("[INFO] Interval DB    : " + String(INTERVAL_DB / 1000) + " detik (Supabase)");
  Serial.println("[INFO] Supabase tabel : " + String(TABLE_NAME));
  Serial.println("========================================");
  Serial.println("  Format: Suhu | Hum | SoilADC | Soil% | pHADC | pH");
  Serial.println("========================================\n");
}

// ════════════════════════════════════════════════════════════════
//  LOOP
// ════════════════════════════════════════════════════════════════
void loop() {
  if (!mqttClient.connected()) koneksiMQTT();
  mqttClient.loop();
  if (WiFi.status() != WL_CONNECTED) koneksiWiFi();

  unsigned long now = millis();
  if (now - lastBacaMillis >= INTERVAL_BACA) {
    lastBacaMillis = now;

    // 1. DHT22
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) { g_suhu = t; g_kelembaban = h; }

    // 2. Soil Moisture
    g_soil = bacaSoil();

    // 3. pH + DMS
    g_pH = bacaPH();

    // 4. Inferensi Fuzzy
    FuzzyOutput fo = inferensiFuzzy(g_suhu, g_soil, g_pH);

    // 5. Aktuator — hanya jika mode otomatis
    if (!manual_mode) {
      terapkanAktuator(fo.kipas, fo.pompa_air, fo.pompa_ph);
    }

    // — Tampilkan sensor + relay dalam 1 baris (setelah relay diterapkan)
    Serial.printf("[%s] Suhu:%.1fC Hum:%.1f%% SoilADC:%d Soil:%.1f%% pHADC:%d pH:%.2f | Kipas:%s Air:%s pH:%s [%s]\n",
      getWaktu().c_str(),
      g_suhu, g_kelembaban, g_adcSoil, g_soil, g_adcPH, g_pH,
      relay_kipas_state     ? "ON" : "OFF",
      relay_pompa_air_state ? "ON" : "OFF",
      relay_pompa_ph_state  ? "ON" : "OFF",
      manual_mode           ? "MANUAL" : "AUTO");

    // 6. Kirim ke MQTT setiap INTERVAL_BACA (15 detik)
    publishMQTT(fo);

    // 7. Kirim ke Supabase setiap INTERVAL_DB (60 detik)
    if (now - lastDBMillis >= INTERVAL_DB) {
      lastDBMillis = now;
      insertSupabase(fo);
    }
  }
}
