/*********
 * Sistem IoT Pertanian Cerdas Cabai Rawit — ESP32 FreeRTOS
 * Board: ESP32 Dev Module (ESP32-30P)
 *
 * Sensor   : DHT22 (GPIO4), Soil (GPIO35), pH+DMS (GPIO34+GPIO13)
 * Aktuator : Relay Kipas (GPIO25), Pompa Air (GPIO27), Pompa pH (GPIO26)
 * Koneksi  : WiFi → MQTT TLS 8883 + Supabase REST
 * Logika   : Fuzzy Tahani
 *
 * Arsitektur RTOS (2 task, 2 core):
 *   Core 0 — taskKomunikasi : MQTT loop + Supabase HTTP (non-blocking)
 *   Core 1 — taskSensor     : baca sensor + fuzzy + relay (boleh blocking)
 *   Sinkronisasi            : xSemaphore mutex + volatile shared data
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
#include <time.h>

// ════════════════════════════════════════════════════════════════
//  KONFIGURASI
// ════════════════════════════════════════════════════════════════
const char* WIFI_SSID   = "UPT-LAB-KOM";
const char* WIFI_PASS   = "uptlab12";
const char* MQTT_SERVER = "n01d3130.ala.asia-southeast1.emqxsl.com";
const int   MQTT_PORT   = 8883;
const char* MQTT_USER   = "pertanian";
const char* MQTT_PASS   = "pertanian12";
const char* TOPIC_PUB   = "pertanian/sensor";
const char* TOPIC_SUB   = "pertanian/kontrol";

#define SUPABASE_URL  "https://sptomqebtvclfebaktof.supabase.co"
#define SUPABASE_KEY  "sb_publishable_jqEF4hY0nK0Bu0BkKK2ayQ_eW_8fh_u"
#define TABLE_NAME    "pertanian"

// ════════════════════════════════════════════════════════════════
//  PIN
// ════════════════════════════════════════════════════════════════
#define DHTPIN  4
#define DHTTYPE DHT22
const int SOIL_PIN      = 35;
const int PH_ADC_PIN    = 34;
const int DMS_PIN       = 13;
const int LED_PIN       = 2;
const int RELAY_KIPAS     = 25;
const int RELAY_POMPA_AIR = 27;
const int RELAY_POMPA_PH  = 26;

// ════════════════════════════════════════════════════════════════
//  KALIBRASI & TIMING
// ════════════════════════════════════════════════════════════════
// ════════════════════════════════════════════════════════════════
//  KALIBRASI SOIL MOISTURE (ADC 12-bit, 0–4095)
//  Sensor ini: ADC BESAR = KERING, ADC KECIL = BASAH
//
//  Data terukur (sensor sudah stabil, tidak basah sisa air):
//    ADC 2188 → tanah kering (alat manual 20%)
//    ADC 1733 → dalam air = 100%
//    Dihitung: SOIL_ADC_KERING = 2302 agar ADC 2188 → 20%
//
//  Cara kalibrasi ulang:
//    1. Tancapkan ke tanah kering, tunggu 2 menit sampai ADC stabil
//       → catat SoilADC: → pakai rumus di atas untuk hitung KERING
//    2. Celupkan ke air → catat SoilADC: → SOIL_ADC_BASAH
//
//  Catatan: ADC sensor butuh 1-2 menit untuk stabil setelah pindah media.
//  Jangan kalibrasi langsung setelah sensor dipindah dari basah ke kering.
// ════════════════════════════════════════════════════════════════
const int SOIL_ADC_KERING = 2302;   // dihitung: ADC 2188 → 20% dengan BASAH=1733
const int SOIL_ADC_BASAH  = 1733;   // ADC sensor dalam air → 100%

const TickType_t INTERVAL_SENSOR_MS = 15000;  // siklus sensor (ms)
const TickType_t INTERVAL_DB_MS     = 60000;  // siklus Supabase (ms)

// Anti-interferensi soil ↔ pH
const unsigned long SOIL_SETTLE_MS = 1500;
const int           SOIL_SAMPLES   = 20;

const unsigned long INTERVAL_PH_ON  = 5000;
const unsigned long INTERVAL_PH_OFF = 2000;

// Pompa Air: nyala 5 detik, jeda 30 MENIT
const unsigned long POMPA_AIR_DURASI = 5000;
const unsigned long POMPA_AIR_JEDA   = 1800000UL;  // 30 menit

// Pompa pH: nyala 5 detik, jeda 3 JAM
const unsigned long POMPA_PH_DURASI  = 5000;
const unsigned long POMPA_PH_JEDA    = 10800000UL; // 3 jam

// ════════════════════════════════════════════════════════════════
//  KALIBRASI pH
//  Celupkan elektroda ke tiap buffer → catat pHADC: di Serial
// ════════════════════════════════════════════════════════════════
// ════════════════════════════════════════════════════════════════
//  KALIBRASI pH — 3 TITIK PIECEWISE LINEAR
//
//  Data aktual (diukur dengan alat manual):
//    ADC 1062 → pH 4.01  (titik asam — belum diverifikasi ulang)
//    ADC  689 → pH 6.86  (dikoreksi: ADC 689 = pH 6.80 alat manual)
//    ADC  520 → pH 9.18  (estimasi proporsional — WAJIB diukur ulang)
//
//  Cara kalibrasi ulang yang benar:
//    1. Celupkan ke buffer pH 4.01 → tunggu stabil → catat pHADC: di Serial
//    2. Celupkan ke buffer pH 6.86 → catat
//    3. Celupkan ke buffer pH 9.18 → catat
//    4. Update tiga baris ADC di bawah
// ════════════════════════════════════════════════════════════════
struct PhCalPoint { int adc; float ph; };
const PhCalPoint PH_CAL[3] = {
  { 1082, 4.01f },   // ← dikoreksi: ADC aktual elektroda di ~pH 4 = 1080-1083
  {  689, 6.86f },   // ← dikoreksi: ADC 689 = pH 6.80 alat manual
  {  520, 9.18f },   // ← estimasi proporsional, ukur ulang dengan buffer 9.18
};

const int   PH_SAMPLES      = 150;
const int   PH_TRIM_PCT     = 25;
const int   PH_MA_SIZE      = 5;       // dikurangi 10→5: buffer lebih kecil, tidak drag nilai lama
const int   PH_MICRO_SIZE   = 3;       // dikurangi 5→3: respons lebih cepat
const float PH_EMA_ALPHA    = 0.35f;   // dinaikkan 0.15→0.35: ikuti nilai baru lebih agresif
const float PH_NOISE_GATE   = 2.00f;   // dinaikkan 1.20→2.00: toleransi interferensi galvanik
                                        // saat sensor soil dan pH dalam 1 wadah (delta ~1.3 unit)
const float PH_RATE_LIMIT   = 0.10f;   // dinaikkan 0.02→0.10: kejar nilai nyata, tidak merangkak lambat
const float PH_HYSTERESIS   = 0.03f;   // dikurangi 0.05→0.03: update lebih sering
const float PH_TEMP_COEF    = 0.003f;
const float PH_TEMP_REF     = 25.0f;
const int   PH_ADC_BATAS    = 1250;  // dinaikkan 1080→1250: ADC normal elektroda ~1085-1090,
                                     // threshold lama terlalu rendah → false "probe lepas"
const int   PH_NOTANCAP_CNT = 5;
const int   PH_VARIANSI_MAX = 250;
const int   PH_WARMUP_N     = 3;       // dinaikkan 2→3: beri lebih banyak siklus untuk stabilkan kalibrasi baru
const int   PH_DRIFT_MIN    = 8;
const int   PH_DRIFT_CNT_MAX= 3;

// ════════════════════════════════════════════════════════════════
//  OBJEK & RTOS HANDLE
// ════════════════════════════════════════════════════════════════
DHT              dht(DHTPIN, DHTTYPE);
WiFiClientSecure tlsClient;
PubSubClient     mqttClient(tlsClient);

// Mutex — melindungi akses ke shared data antara taskSensor dan taskKomunikasi
SemaphoreHandle_t xMutex = NULL;

// Notifikasi: taskSensor memberi tahu taskKomunikasi ada data baru
TaskHandle_t hTaskKom   = NULL;
TaskHandle_t hTaskSensor= NULL;

// ════════════════════════════════════════════════════════════════
//  SHARED DATA — hanya diakses dengan mutex
//  taskSensor menulis, taskKomunikasi membaca
// ════════════════════════════════════════════════════════════════
struct SharedData {
  float suhu       = 0.0f;
  float kelembaban = 0.0f;
  float soil       = 0.0f;
  float pH         = 0.0f;
  int   adcPH      = 0;
  int   adcSoil    = 0;
  bool  relay_kipas     = false;
  bool  relay_pompa_air = false;
  bool  relay_pompa_ph  = false;
  float mu_kipas    = 0.0f;
  float mu_pompa_air= 0.0f;
  float mu_pompa_ph = 0.0f;
  const char* status_suhu  = "–";
  const char* status_tanah = "–";
  const char* status_ph    = "–";
  bool  manual_mode = false;
  // Countdown jeda pompa (detik tersisa sebelum boleh nyala lagi)
  // 0 = siap nyala, >0 = masih dalam jeda, -1 = sedang nyala
  int countdown_air = 0;
  int countdown_ph  = 0;
};
SharedData sd;

// Manual override — ditulis oleh MQTT callback (Core 0), dibaca taskSensor
struct ManualCmd {
  bool aktif      = false;
  bool kipas      = false;
  bool pompa_air  = false;
  bool pompa_ph   = false;
};
volatile ManualCmd manualCmd;

// Flag: ada data baru yang perlu dikirim ke MQTT/Supabase
volatile bool flagDataBaru   = false;
volatile bool flagKirimDB    = false;

// ════════════════════════════════════════════════════════════════
//  STRUCT FUZZY OUTPUT
// ════════════════════════════════════════════════════════════════
struct FuzzyOutput {
  bool  kipas, pompa_air, pompa_ph;
  float mu_kipas, mu_pompa_air, mu_pompa_ph;
  const char* status_suhu;
  const char* status_tanah;
  const char* status_ph;
};

// ════════════════════════════════════════════════════════════════
//  FUNGSI KEANGGOTAAN — SUHU
// ════════════════════════════════════════════════════════════════
static float fuzzyTempRendah(float x) {
  if (x <= 24.0f) return 1.0f;
  if (x < 27.0f)  return (27.0f - x) / 3.0f;
  return 0.0f;
}
static float fuzzyTempSedang(float x) {
  if (x <= 24.0f || x >= 31.0f) return 0.0f;
  if (x <= 27.0f) return (x - 24.0f) / 3.0f;
  return (31.0f - x) / 4.0f;
}
static float fuzzyTempTinggi(float x) {
  if (x <= 27.0f) return 0.0f;
  if (x < 31.0f)  return (x - 27.0f) / 4.0f;
  return 1.0f;
}
static const char* fuzzyTempStatus(float x) {
  float r=fuzzyTempRendah(x), s=fuzzyTempSedang(x), t=fuzzyTempTinggi(x);
  if (r>=s && r>=t) return "Rendah";
  if (s>=t)         return "Sedang";
  return "Tinggi";
}

// ════════════════════════════════════════════════════════════════
//  FUNGSI KEANGGOTAAN — SOIL
// ════════════════════════════════════════════════════════════════
static float fuzzySoilKering(float x) {
  if (x <= 40.0f) return 1.0f;
  if (x < 50.0f)  return (50.0f - x) / 10.0f;
  return 0.0f;
}
static float fuzzySoilLembab(float x) {
  if (x <= 40.0f || x >= 80.0f) return 0.0f;
  if (x < 50.0f)  return (x - 40.0f) / 10.0f;
  if (x <= 70.0f) return 1.0f;
  return (80.0f - x) / 10.0f;
}
static float fuzzySoilBasah(float x) {
  if (x <= 70.0f) return 0.0f;
  if (x < 80.0f)  return (x - 70.0f) / 10.0f;
  return 1.0f;
}
static const char* fuzzySoilStatus(float x) {
  float k=fuzzySoilKering(x), l=fuzzySoilLembab(x), b=fuzzySoilBasah(x);
  if (k>=l && k>=b) return "Kering";
  if (l>=b)         return "Lembab";
  return "Basah";
}

// ════════════════════════════════════════════════════════════════
//  FUNGSI KEANGGOTAAN — pH
// ════════════════════════════════════════════════════════════════
static float fuzzyPhAsam(float x) {
  if (x <= 5.0f) return 1.0f;
  if (x < 6.0f)  return (6.0f - x);
  return 0.0f;
}
static float fuzzyPhNormal(float x) {
  if (x <= 5.5f || x >= 7.5f) return 0.0f;
  if (x < 6.0f)  return (x - 5.5f) / 0.5f;
  if (x <= 7.0f) return 1.0f;
  return (7.5f - x) / 0.5f;
}
static float fuzzyPhBasa(float x) {
  if (x <= 7.0f) return 0.0f;
  if (x < 7.5f)  return (x - 7.0f) / 0.5f;
  return 1.0f;
}
static const char* fuzzyPhStatus(float x) {
  float a=fuzzyPhAsam(x), n=fuzzyPhNormal(x), b=fuzzyPhBasa(x);
  if (a>=n && a>=b) return "Asam";
  if (n>=b)         return "Normal";
  return "Basa";
}

// ════════════════════════════════════════════════════════════════
//  INFERENSI FUZZY TAHANI
// ════════════════════════════════════════════════════════════════
FuzzyOutput inferensiFuzzy(float suhu, float soil, float pH) {
  FuzzyOutput out;
  float mu_st = fuzzyTempTinggi(suhu);
  float mu_tk = fuzzySoilKering(soil);
  float mu_pa = fuzzyPhAsam(pH);

  out.status_suhu  = fuzzyTempStatus(suhu);
  out.status_tanah = fuzzySoilStatus(soil);
  out.status_ph    = fuzzyPhStatus(pH);

  // R1: suhu Tinggi → Kipas ON
  out.mu_kipas = mu_st;
  out.kipas    = (mu_st > 0.5f);

  // R2: tanah Kering → Pompa Air ON
  out.mu_pompa_air = (soil <= 0.0f) ? 0.0f : mu_tk;
  out.pompa_air    = (soil > 0.0f && out.mu_pompa_air > 0.4f);

  // R3: pH Asam → Pompa pH ON
  out.mu_pompa_ph = (pH <= 0.0f) ? 0.0f : mu_pa;
  out.pompa_ph    = (pH > 0.0f && mu_pa > 0.4f);

  return out;
}

// ════════════════════════════════════════════════════════════════
//  FILTER pH — STATE (hanya diakses dari taskSensor / Core 1)
// ════════════════════════════════════════════════════════════════
static float phMicro[3]  = {0};   // ukuran sesuai PH_MICRO_SIZE=3
static int   phMicro_idx = 0;
static bool  phMicro_full= false;
static float phMA[5]     = {0};   // ukuran sesuai PH_MA_SIZE=5
static int   phMA_idx    = 0;
static bool  phMA_full   = false;
static float phEMA       = 0.0f;
static float phLast      = 0.0f;
static float phOutput    = 0.0f;
static int   phNotTancapCnt = 0;
static int   phWarmupCount  = 0;
static int   adcPrevious    = 0;
static int   phDriftCount   = 0;

static void phResetState() {
  phLast = phOutput = phEMA = 0.0f;
  phMicro_idx = phMA_idx = 0;
  phMicro_full = phMA_full = false;
  phWarmupCount = 0;
  phDriftCount  = 0;
}

static void sortArray(int* arr, int n) {
  for (int i = 1; i < n; i++) {
    int key = arr[i], j = i - 1;
    while (j >= 0 && arr[j] > key) { arr[j+1] = arr[j]; j--; }
    arr[j+1] = key;
  }
}

static float adcToPH(int adc) {
  // Urutkan titik kalibrasi ascending by ADC
  int idx[3] = {0,1,2};
  for (int i=0;i<2;i++)
    for (int j=0;j<2-i;j++)
      if (PH_CAL[idx[j]].adc > PH_CAL[idx[j+1]].adc) { int t=idx[j]; idx[j]=idx[j+1]; idx[j+1]=t; }
  int   a0=PH_CAL[idx[0]].adc, a1=PH_CAL[idx[1]].adc, a2=PH_CAL[idx[2]].adc;
  float p0=PH_CAL[idx[0]].ph,  p1=PH_CAL[idx[1]].ph,  p2=PH_CAL[idx[2]].ph;
  if      (adc <= a0) return p0 + (p1-p0)/(float)(a1-a0) * (adc-a0);
  else if (adc <= a1) return p0 + (float)(adc-a0)/(a1-a0) * (p1-p0);
  else if (adc <= a2) return p1 + (float)(adc-a1)/(a2-a1) * (p2-p1);
  else                return p1 + (p2-p1)/(float)(a2-a1) * (adc-a1);
}

static float kompensasiSuhu(float ph, float suhu) {
  return ph - (PH_TEMP_COEF * (suhu - PH_TEMP_REF) * (ph - 7.0f));
}

// ════════════════════════════════════════════════════════════════
//  BACA pH — dijalankan di Core 1 (taskSensor), boleh blocking
//  Parameter suhu diambil dari local copy agar thread-safe
// ════════════════════════════════════════════════════════════════
float bacaPH(float suhuLokal) {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // DMS ON — blocking murni, Core 0 tetap proses MQTT independen
  digitalWrite(DMS_PIN, LOW);
  digitalWrite(LED_PIN, HIGH);
  vTaskDelay(pdMS_TO_TICKS(INTERVAL_PH_ON));

  // 150 sampel
  int s[PH_SAMPLES];
  for (int i = 0; i < PH_SAMPLES; i++) {
    s[i] = analogRead(PH_ADC_PIN);
    vTaskDelay(pdMS_TO_TICKS(5));
  }

  digitalWrite(DMS_PIN, HIGH);
  digitalWrite(LED_PIN, LOW);
  vTaskDelay(pdMS_TO_TICKS(INTERVAL_PH_OFF));

  sortArray(s, PH_SAMPLES);

  // Cek variansi (5% trimmed)
  int varN     = PH_SAMPLES * 5 / 100;
  int variansi = s[PH_SAMPLES-1-varN] - s[varN];
  if (variansi > PH_VARIANSI_MAX) {
    Serial.printf("[pH] Variansi=%d → tidak stabil\n", variansi);
    phResetState(); return 0.0f;
  }

  int adcMed  = (s[74] + s[75]) / 2;
  int trimN   = PH_SAMPLES * PH_TRIM_PCT / 100;
  long sumT   = 0; int cntT = 0;
  for (int i = trimN; i < PH_SAMPLES-trimN; i++) { sumT += s[i]; cntT++; }
  int adcTrim = (int)(sumT / cntT);

  // Spike check
  int adcFinal = (abs(adcTrim-adcMed) > 50) ? adcTrim
                 : (int)(adcTrim*0.60f + adcMed*0.40f);

  // Update global ADC pH dengan mutex
  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    sd.adcPH = adcFinal;
    xSemaphoreGive(xMutex);
  }

  // Cek probe tidak terpasang
  if (adcFinal >= PH_ADC_BATAS) {
    phNotTancapCnt++;
    if (phNotTancapCnt >= PH_NOTANCAP_CNT) {
      Serial.printf("[pH] Probe lepas (ADC=%d)\n", adcFinal);
      phResetState(); return 0.0f;
    }
    return phLast;
  }
  phNotTancapCnt = 0;

  // Cek drift naik (mengering)
  if (adcPrevious > 0 && adcFinal > adcPrevious + PH_DRIFT_MIN) {
    if (++phDriftCount >= PH_DRIFT_CNT_MAX) {
      Serial.printf("[pH] Drift naik → reset\n");
      phResetState(); adcPrevious = adcFinal; return 0.0f;
    }
  } else { phDriftCount = 0; }
  adcPrevious = adcFinal;

  float phRaw  = adcToPH(adcFinal);
  if (phRaw < 0.0f || phRaw > 14.0f) { phResetState(); return 0.0f; }
  float phComp = kompensasiSuhu(phRaw, suhuLokal);

  // Noise gate
  if (phLast > 0.0f && fabsf(phComp - phLast) > PH_NOISE_GATE) {
    Serial.printf("[pH] Noise gate delta=%.3f → reset\n", fabsf(phComp-phLast));
    phResetState(); return 0.0f;
  }

  // Warmup — isi semua buffer sekaligus
  if (phWarmupCount < PH_WARMUP_N) {
    phWarmupCount++;
    Serial.printf("[pH] Warmup %d/%d ADC=%d phRaw=%.3f\n",
      phWarmupCount, PH_WARMUP_N, adcFinal, phRaw);
    for (int i=0;i<PH_MICRO_SIZE;i++) phMicro[i]=phComp;
    phMicro_idx=0; phMicro_full=true;
    for (int i=0;i<PH_MA_SIZE;i++) phMA[i]=phComp;
    phMA_idx=0; phMA_full=true;
    phEMA=phOutput=phLast=phComp;
    return 0.0f;
  }

  // Micro-buffer
  phMicro[phMicro_idx] = phComp;
  phMicro_idx = (phMicro_idx+1) % PH_MICRO_SIZE;
  if (phMicro_idx == 0) phMicro_full = true;
  int   mcnt = phMicro_full ? PH_MICRO_SIZE : phMicro_idx;
  float phMV = 0.0f;
  for (int i=0;i<mcnt;i++) phMV += phMicro[i];
  phMV /= mcnt;

  // Weighted MA
  phMA[phMA_idx] = phMV;
  phMA_idx = (phMA_idx+1) % PH_MA_SIZE;
  if (phMA_idx == 0) phMA_full = true;
  int   macnt = phMA_full ? PH_MA_SIZE : phMA_idx;
  float phMAv = 0.0f, wTot = 0.0f;
  for (int i=0;i<macnt;i++) {
    int   wi = (phMA_idx - macnt + i + PH_MA_SIZE) % PH_MA_SIZE;
    float w  = 1.0f + (float)i;
    phMAv += phMA[wi] * w;
    wTot  += w;
  }
  phMAv /= wTot;

  // EMA
  phEMA = (phEMA == 0.0f) ? phMAv : PH_EMA_ALPHA * phMAv + (1.0f-PH_EMA_ALPHA)*phEMA;

  // Rate limiter + hysteresis
  if (phOutput == 0.0f) {
    phOutput = phEMA;
  } else if (fabsf(phEMA - phOutput) > PH_HYSTERESIS) {
    float diff = phEMA - phOutput;
    phOutput += (fabsf(diff) > PH_RATE_LIMIT)
      ? ((diff>0) ? PH_RATE_LIMIT : -PH_RATE_LIMIT) : diff;
  }
  phLast = phOutput;

  Serial.printf("[pH] ADCtrim=%d ADCfinal=%d var=%d "
                "phRaw=%.3f phComp=%.3f phMA=%.3f phEMA=%.3f phOut=%.3f\n",
    adcTrim, adcFinal, variansi, phRaw, phComp, phMAv, phEMA, phOutput);
  return phOutput;
}

// ════════════════════════════════════════════════════════════════
//  BACA SOIL — dijalankan di Core 1, dengan settling delay
// ════════════════════════════════════════════════════════════════
float bacaSoil() {
  analogReadResolution(12);
  long sumRaw = 0;
  for (int i=0;i<SOIL_SAMPLES;i++) {
    sumRaw += analogRead(SOIL_PIN);
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  int raw = (int)(sumRaw / SOIL_SAMPLES);

  // Update shared
  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    sd.adcSoil = raw;
    xSemaphoreGive(xMutex);
  }

  float pct    = map(raw, SOIL_ADC_KERING, SOIL_ADC_BASAH, 0, 100);
  float result = constrain(pct, 0.0f, 100.0f);

  Serial.printf("[Soil] ADC=%d → %.1f%% | Settling %lums...\n",
    raw, result, SOIL_SETTLE_MS);

  // Settling delay — vTaskDelay tidak blokir Core 0
  vTaskDelay(pdMS_TO_TICKS(SOIL_SETTLE_MS));
  return result;
}

// ════════════════════════════════════════════════════════════════
//  KONTROL RELAY (aktif LOW)
// ════════════════════════════════════════════════════════════════
void setRelay(int pin, bool aktif) {
  digitalWrite(pin, aktif ? LOW : HIGH);
}

// State timer pompa — hanya diakses dari taskSensor (Core 1)
static unsigned long pompaAirOnTime = 0, pompaAirOffTime = 0;
static bool          pompaAirNyala  = false;
static unsigned long pompaPHOnTime  = 0, pompaPHOffTime  = 0;
static bool          pompaPHNyala   = false;
// Permintaan nyala dari fuzzy/manual — diset terapkanAktuator, dicek tickPompa
static bool reqPompaAir = false;
static bool reqPompaPH  = false;

// ── tickPompa: dipanggil setiap 50ms dari taskSensor loop ────────
// Mengelola timer ON/OFF pompa secara independen dari siklus sensor.
// Dengan cara ini relay tidak bisa nyala dobel akibat pemanggilan
// terapkanAktuator dari dua tempat (fast-path + siklus sensor).
void tickPompa() {
  unsigned long now = millis();
  int cdAir = 0, cdPH = 0;

  // ── Pompa Air ────────────────────────────────────────────────
  if (reqPompaAir && !pompaAirNyala) {
    // Ada permintaan ON dan sedang tidak nyala — cek jeda
    if (pompaAirOffTime == 0 || now - pompaAirOffTime >= POMPA_AIR_JEDA) {
      setRelay(RELAY_POMPA_AIR, true);
      pompaAirNyala  = true;
      pompaAirOnTime = now;
      Serial.println("[PompaAir] ON — nyala 5 detik");
    } else {
      unsigned long sisa = POMPA_AIR_JEDA - (now - pompaAirOffTime);
      cdAir = (int)(sisa / 1000);
    }
  }
  if (pompaAirNyala) {
    cdAir = -1;
    if (now - pompaAirOnTime >= POMPA_AIR_DURASI) {
      // Durasi 5 detik habis — matikan
      setRelay(RELAY_POMPA_AIR, false);
      pompaAirNyala   = false;
      pompaAirOffTime = now;
      reqPompaAir     = false;  // reset permintaan, tunggu siklus fuzzy berikutnya
      Serial.printf("[PompaAir] OFF — jeda 30 menit (%lu detik)\n", POMPA_AIR_JEDA/1000);
      cdAir = (int)(POMPA_AIR_JEDA / 1000);
    }
  }
  if (!reqPompaAir && !pompaAirNyala) {
    setRelay(RELAY_POMPA_AIR, false);
    if (pompaAirOffTime > 0 && now - pompaAirOffTime < POMPA_AIR_JEDA)
      cdAir = (int)((POMPA_AIR_JEDA - (now - pompaAirOffTime)) / 1000);
  }

  // ── Pompa pH ─────────────────────────────────────────────────
  if (reqPompaPH && !pompaPHNyala) {
    if (pompaPHOffTime == 0 || now - pompaPHOffTime >= POMPA_PH_JEDA) {
      setRelay(RELAY_POMPA_PH, true);
      pompaPHNyala  = true;
      pompaPHOnTime = now;
      Serial.println("[PompaPH] ON — nyala 5 detik");
    } else {
      unsigned long sisa = POMPA_PH_JEDA - (now - pompaPHOffTime);
      cdPH = (int)(sisa / 1000);
    }
  }
  if (pompaPHNyala) {
    cdPH = -1;
    if (now - pompaPHOnTime >= POMPA_PH_DURASI) {
      setRelay(RELAY_POMPA_PH, false);
      pompaPHNyala   = false;
      pompaPHOffTime = now;
      reqPompaPH     = false;
      Serial.printf("[PompaPH] OFF — jeda 3 jam (%lu menit)\n", POMPA_PH_JEDA/60000);
      cdPH = (int)(POMPA_PH_JEDA / 1000);
    }
  }
  if (!reqPompaPH && !pompaPHNyala) {
    setRelay(RELAY_POMPA_PH, false);
    if (pompaPHOffTime > 0 && now - pompaPHOffTime < POMPA_PH_JEDA)
      cdPH = (int)((POMPA_PH_JEDA - (now - pompaPHOffTime)) / 1000);
  }

  // Log sisa jeda ke Serial
  if (cdAir > 0) {
    // Cetak hanya tiap ~60 detik untuk tidak spam Serial
    static unsigned long lastLogAir = 0;
    if (now - lastLogAir >= 60000UL) {
      lastLogAir = now;
      int m = cdAir / 60, s = cdAir % 60;
      Serial.printf("[PompaAir] Jeda: %02d:%02d (sisa %d detik)\n", m, s, cdAir);
    }
  }
  if (cdPH > 0) {
    static unsigned long lastLogPH = 0;
    if (now - lastLogPH >= 60000UL) {
      lastLogPH = now;
      int h = cdPH / 3600, m = (cdPH % 3600) / 60, s = cdPH % 60;
      Serial.printf("[PompaPH]  Jeda: %02d:%02d:%02d (sisa %d detik)\n", h, m, s, cdPH);
    }
  }

  // Update countdown ke shared data
  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    sd.countdown_air = cdAir;
    sd.countdown_ph  = cdPH;
    xSemaphoreGive(xMutex);
  }
}

// ── terapkanAktuator: set permintaan relay, TIDAK langsung nyalakan pompa ──
// Kipas langsung dikontrol di sini. Pompa dikontrol via tickPompa().
void terapkanAktuator(bool kipas, bool pompa_air, bool pompa_ph) {
  setRelay(RELAY_KIPAS, kipas);
  // Set permintaan — tickPompa() yang akan memutuskan kapan relay nyala
  reqPompaAir = pompa_air;
  reqPompaPH  = pompa_ph;
}

// ════════════════════════════════════════════════════════════════
//  NTP
// ════════════════════════════════════════════════════════════════
#define NTP_SERVER "pool.ntp.org"
#define NTP_OFFSET 25200   // UTC+7

void ntpSync() {
  configTime(NTP_OFFSET, 0, NTP_SERVER);
  struct tm ti;
  unsigned long t = millis();
  while (!getLocalTime(&ti) && millis()-t < 5000) delay(200);
  if (ti.tm_year+1900 > 2020)
    Serial.printf("[NTP] %02d/%02d/%04d %02d:%02d:%02d WIB\n",
      ti.tm_mday, ti.tm_mon+1, ti.tm_year+1900, ti.tm_hour, ti.tm_min, ti.tm_sec);
  else
    Serial.println("[NTP] Gagal sync");
}

String getWaktu() {
  struct tm ti;
  if (!getLocalTime(&ti)) return "--:--:--";
  char buf[12]; strftime(buf, sizeof(buf), "%H:%M:%S", &ti);
  return String(buf);
}

String getWaktuISO() {
  struct tm ti;
  if (!getLocalTime(&ti)) return "";
  char buf[30]; strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S+07:00", &ti);
  return String(buf);
}

// ════════════════════════════════════════════════════════════════
//  PUBLISH MQTT — dipanggil dari taskKomunikasi (Core 0)
// ════════════════════════════════════════════════════════════════
void publishMQTT() {
  if (!mqttClient.connected()) return;

  // Ambil snapshot shared data dengan mutex
  SharedData snap;
  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    snap = sd;
    xSemaphoreGive(xMutex);
  } else { return; }

  StaticJsonDocument<768> doc;
  doc["temperature"]  = snap.suhu;
  doc["humidity"]     = snap.kelembaban;
  doc["soil"]         = snap.soil;
  doc["ph"]           = snap.pH;
  doc["fuzzy_suhu"]   = snap.mu_kipas;
  doc["fuzzy_soil"]   = snap.mu_pompa_air;
  doc["fuzzy_ph"]     = snap.mu_pompa_ph;
  doc["relay_kipas"]  = snap.relay_kipas     ? 1.0f : 0.0f;
  doc["relay_air"]    = snap.relay_pompa_air ? 1.0f : 0.0f;
  doc["relay_ph"]     = snap.relay_pompa_ph  ? 1.0f : 0.0f;
  doc["status_suhu"]  = snap.status_suhu;
  doc["status_tanah"] = snap.status_tanah;
  doc["status_ph"]    = snap.status_ph;
  doc["adc_ph"]       = snap.adcPH;
  doc["adc_soil"]     = snap.adcSoil;
  doc["waktu"]        = getWaktu();
  doc["manual_mode"]  = snap.manual_mode;
  doc["countdown_air"]= snap.countdown_air;  // detik sisa jeda pompa air (-1=nyala, 0=siap)
  doc["countdown_ph"] = snap.countdown_ph;   // detik sisa jeda pompa pH
  doc["wifi_ssid"]    = WiFi.SSID();
  doc["wifi_ip"]      = WiFi.localIP().toString();
  doc["wifi_rssi"]    = WiFi.RSSI();
  doc["wifi_status"]  = (WiFi.status() == WL_CONNECTED) ? "Terhubung" : "Terputus";

  char buf[768];
  serializeJson(doc, buf);
  mqttClient.setBufferSize(768);
  mqttClient.publish(TOPIC_PUB, buf, true);
}

// ════════════════════════════════════════════════════════════════
//  INSERT SUPABASE — dipanggil dari taskKomunikasi (Core 0)
// ════════════════════════════════════════════════════════════════
void insertSupabase() {
  SharedData snap;
  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    snap = sd;
    xSemaphoreGive(xMutex);
  } else { return; }

  StaticJsonDocument<448> doc;
  doc["temperature"] = snap.suhu;
  doc["humidity"]    = snap.kelembaban;
  doc["soil"]        = snap.soil;
  doc["ph"]          = snap.pH;
  doc["fuzzy_suhu"]  = snap.mu_kipas;
  doc["fuzzy_soil"]  = snap.mu_pompa_air;
  doc["fuzzy_ph"]    = snap.mu_pompa_ph;
  doc["relay_kipas"] = snap.relay_kipas     ? 1.0f : 0.0f;
  doc["relay_air"]   = snap.relay_pompa_air ? 1.0f : 0.0f;
  doc["relay_ph"]    = snap.relay_pompa_ph  ? 1.0f : 0.0f;
  String iso = getWaktuISO();
  if (iso.length() > 0) doc["updated_at"] = iso;

  char body[448];
  serializeJson(doc, body);

  WiFiClientSecure sbClient;
  sbClient.setInsecure();
  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/" + TABLE_NAME;
  if (!http.begin(sbClient, url)) { Serial.println("[Supabase] gagal"); return; }
  http.setTimeout(10000);
  http.addHeader("Content-Type",  "application/json");
  http.addHeader("apikey",        SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("Prefer",        "return=minimal");
  int code = http.POST(body);
  if (code != 201) Serial.println("[Supabase] Error " + String(code));
  http.end();
  sbClient.stop();
}

// ════════════════════════════════════════════════════════════════
//  MQTT CALLBACK — dipanggil dari Core 0, tulis manualCmd (volatile)
// ════════════════════════════════════════════════════════════════
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<200> doc;
  if (deserializeJson(doc, payload, length)) return;

  bool newManual = manualCmd.aktif;
  if (doc.containsKey("manual")) newManual = doc["manual"].as<bool>();
  manualCmd.aktif = newManual;

  if (newManual) {
    if (doc.containsKey("kipas"))     manualCmd.kipas     = doc["kipas"].as<bool>();
    if (doc.containsKey("pompa_air")) manualCmd.pompa_air = doc["pompa_air"].as<bool>();
    if (doc.containsKey("pompa_ph"))  manualCmd.pompa_ph  = doc["pompa_ph"].as<bool>();
    Serial.printf("[Manual] Kipas:%s Air:%s pH:%s\n",
      manualCmd.kipas?"ON":"OFF", manualCmd.pompa_air?"ON":"OFF", manualCmd.pompa_ph?"ON":"OFF");
  } else {
    Serial.println("[Manual] Mode otomatis");
  }
  // Tandai data baru agar taskKomunikasi kirim feedback segera
  flagDataBaru = true;
}

// ════════════════════════════════════════════════════════════════
//  KONEKSI WiFi
// ════════════════════════════════════════════════════════════════
void koneksiWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-t < 15000) delay(500);
  if (WiFi.status() != WL_CONNECTED) ESP.restart();
}

// ════════════════════════════════════════════════════════════════
//  KONEKSI MQTT
// ════════════════════════════════════════════════════════════════
void koneksiMQTT() {
  tlsClient.setInsecure();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(640);
  String id = "ESP32-Cabai-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  while (!mqttClient.connected()) {
    if (mqttClient.connect(id.c_str(), MQTT_USER, MQTT_PASS)) {
      Serial.println("[MQTT] Terhubung");
      mqttClient.subscribe(TOPIC_SUB);
    } else {
      Serial.print(".");
      delay(3000);
    }
  }
}

// ════════════════════════════════════════════════════════════════
//  TASK KOMUNIKASI — Core 0, prioritas 1
//  Tugas: MQTT keep-alive + kirim data + Supabase
//  Tidak ada blocking I/O sensor di sini
// ════════════════════════════════════════════════════════════════
void taskKomunikasi(void* param) {
  // Inisialisasi WiFi + MQTT di core ini
  Serial.print("[WiFi] Menghubungkan...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-t0 < 15000) {
    vTaskDelay(pdMS_TO_TICKS(500)); Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" GAGAL! Restart..."); ESP.restart();
  }
  Serial.printf(" OK — IP: %s RSSI: %d dBm\n",
    WiFi.localIP().toString().c_str(), WiFi.RSSI());
  ntpSync();
  koneksiMQTT();

  // Beritahu taskSensor bahwa koneksi sudah siap
  xTaskNotifyGive(hTaskSensor);

  TickType_t lastDB = xTaskGetTickCount();

  for (;;) {
    // Reconnect jika putus
    if (WiFi.status() != WL_CONNECTED) koneksiWiFi();
    if (!mqttClient.connected()) koneksiMQTT();

    // MQTT keep-alive — dipanggil sesering mungkin
    mqttClient.loop();

    // Kirim MQTT jika ada data baru dari taskSensor
    if (flagDataBaru) {
      flagDataBaru = false;
      publishMQTT();
    }

    // Kirim Supabase tiap INTERVAL_DB_MS
    if (xTaskGetTickCount() - lastDB >= pdMS_TO_TICKS(INTERVAL_DB_MS)) {
      lastDB = xTaskGetTickCount();
      insertSupabase();
    }

    vTaskDelay(pdMS_TO_TICKS(10));  // yield 10ms — MQTT loop tetap responsif
  }
}

// ════════════════════════════════════════════════════════════════
//  TASK SENSOR — Core 1, prioritas 2
//  Tugas: baca sensor → fuzzy → relay → update shared data
//  Boleh blocking (vTaskDelay) tanpa pengaruh ke MQTT
// ════════════════════════════════════════════════════════════════
void taskSensor(void* param) {
  // Tunggu taskKomunikasi selesai setup WiFi/MQTT
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  Serial.println("[Sensor] Task mulai");

  TickType_t lastSensor = xTaskGetTickCount();
  // Simpan state manual terakhir untuk deteksi perubahan
  bool prevManualAktif = false;
  bool prevKipas = false, prevAir = false, prevPH = false;

  for (;;) {
    // ── Fast-path: relay manual dieksekusi SEGERA tanpa tunggu sensor ──
    // Cek apakah ada perubahan perintah manual sejak iterasi terakhir.
    // Ini berjalan setiap 50ms, jauh lebih cepat dari siklus sensor ~22 detik.
    bool curManual = manualCmd.aktif;
    bool curKipas  = manualCmd.kipas;
    bool curAir    = manualCmd.pompa_air;
    bool curPH     = manualCmd.pompa_ph;

    bool manualBerubah = (curManual != prevManualAktif)
                      || (curManual && (curKipas != prevKipas
                                     || curAir   != prevAir
                                     || curPH    != prevPH));
    if (manualBerubah) {
      prevManualAktif = curManual;
      prevKipas = curKipas; prevAir = curAir; prevPH = curPH;

      if (curManual) {
        // Terapkan relay manual — set permintaan, tickPompa() yang eksekusi
        terapkanAktuator(curKipas, curAir, curPH);
        // Update shared data relay agar dashboard segera dapat feedback
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
          sd.relay_kipas = curKipas;
          // Pompa air/ph: state aktual dikelola tickPompa, tapi set request-nya
          sd.relay_pompa_air = curAir && (pompaAirNyala || reqPompaAir);
          sd.relay_pompa_ph  = curPH  && (pompaPHNyala  || reqPompaPH);
          sd.manual_mode     = true;
          xSemaphoreGive(xMutex);
        }
        flagDataBaru = true;
        Serial.printf("[Manual-Fast] Kipas:%s Air:%s pH:%s\n",
          curKipas?"ON":"OFF", curAir?"ON":"OFF", curPH?"ON":"OFF");
      } else {
        // Kembali ke otomatis — relay akan dikendalikan siklus sensor berikutnya
        Serial.println("[Manual-Fast] Kembali ke AUTO");
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
          sd.manual_mode = false;
          xSemaphoreGive(xMutex);
        }
        flagDataBaru = true;
      }
    }

    // ── Siklus sensor normal: baca semua sensor + fuzzy ──
    TickType_t now = xTaskGetTickCount();
    if (now - lastSensor >= pdMS_TO_TICKS(INTERVAL_SENSOR_MS)) {
      lastSensor = now;

      // 1. DHT22
      float t = dht.readTemperature();
      float h = dht.readHumidity();

      // 2. Soil — dengan settling delay internal
      float soilVal = bacaSoil();

      // 3. pH — ambil suhu lokal dulu, lalu baca (boleh blocking)
      float suhuLokal = (!isnan(t)) ? t : 0.0f;
      float phBaru    = bacaPH(suhuLokal);

      // Cross-contamination guard: tolak jika soil >85% dan pH lompat >1.5
      float phSekarang;
      if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        phSekarang = sd.pH;
        xSemaphoreGive(xMutex);
      } else { phSekarang = 0.0f; }

      if (soilVal > 85.0f && phSekarang > 0.0f && fabsf(phBaru - phSekarang) > 1.5f) {
        Serial.printf("[pH] Cross-guard soil=%.1f%% delta=%.3f → pakai lama\n",
          soilVal, fabsf(phBaru - phSekarang));
        phBaru = phSekarang;
      }

      // 4. Fuzzy inferensi
      FuzzyOutput fo = inferensiFuzzy(suhuLokal, soilVal, phBaru);

      // 5. Terapkan aktuator — mode manual sudah ditangani fast-path di atas,
      //    di sini hanya terapkan jika mode otomatis
      bool isManual = manualCmd.aktif;
      bool ki = isManual ? manualCmd.kipas     : fo.kipas;
      bool ai = isManual ? manualCmd.pompa_air : fo.pompa_air;
      bool pi = isManual ? manualCmd.pompa_ph  : fo.pompa_ph;
      terapkanAktuator(ki, ai, pi);

      // 6. Update shared data dengan mutex
      if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (!isnan(t)) { sd.suhu = t; sd.kelembaban = h; }
        sd.soil          = soilVal;
        sd.pH            = phBaru;
        sd.mu_kipas      = fo.mu_kipas;
        sd.mu_pompa_air  = fo.mu_pompa_air;
        sd.mu_pompa_ph   = fo.mu_pompa_ph;
        sd.status_suhu   = fo.status_suhu;
        sd.status_tanah  = fo.status_tanah;
        sd.status_ph     = fo.status_ph;
        sd.relay_kipas     = ki;
        sd.relay_pompa_air = pompaAirNyala;  // state aktual dari tickPompa
        sd.relay_pompa_ph  = pompaPHNyala;   // state aktual dari tickPompa
        sd.manual_mode     = isManual;
        xSemaphoreGive(xMutex);
      }

      // 7. Beritahu taskKomunikasi ada data baru
      flagDataBaru = true;

      // 8. Log serial
      Serial.printf("[%s] Suhu:%.1fC Hum:%.1f%% SoilADC:%d Soil:%.1f%% pHADC:%d pH:%.2f"
                    " | Kipas:%s Air:%s pH:%s [%s]\n",
        getWaktu().c_str(), suhuLokal, h,
        sd.adcSoil, soilVal, sd.adcPH, phBaru,
        ki?"ON":"OFF", ai?"ON":"OFF", pi?"ON":"OFF",
        isManual?"MANUAL":"AUTO");
    }

    vTaskDelay(pdMS_TO_TICKS(50));  // yield 50ms — fast-path relay cek tiap 50ms
    tickPompa();  // timer pompa berjalan independen dari siklus sensor
  }
}

// ════════════════════════════════════════════════════════════════
//  SETUP — hanya inisialisasi hardware, task dibuat di sini
// ════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n========================================");
  Serial.println("  Pertanian Cerdas Cabai — FreeRTOS");
  Serial.println("========================================");

  // Pin
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

  // Buat mutex sebelum task dijalankan
  xMutex = xSemaphoreCreateMutex();
  if (xMutex == NULL) {
    Serial.println("[FATAL] Mutex gagal dibuat!");
    ESP.restart();
  }

  // Task Sensor — Core 1, prioritas 2, stack 8KB
  xTaskCreatePinnedToCore(
    taskSensor,
    "TaskSensor",
    8192,
    NULL,
    2,
    &hTaskSensor,
    1   // Core 1
  );

  // Task Komunikasi — Core 0, prioritas 1, stack 10KB
  // (stack lebih besar karena WiFiClientSecure + HTTP butuh heap lebih)
  xTaskCreatePinnedToCore(
    taskKomunikasi,
    "TaskKom",
    10240,
    NULL,
    1,
    &hTaskKom,
    0   // Core 0
  );

  Serial.println("[RTOS] Task dibuat:");
  Serial.println("  Core 0 — TaskKom    : MQTT + Supabase");
  Serial.println("  Core 1 — TaskSensor : DHT + Soil + pH + Fuzzy + Relay");
  Serial.println("========================================");
}

// ════════════════════════════════════════════════════════════════
//  LOOP — tidak dipakai, semua logika ada di task
//  FreeRTOS tetap membutuhkan loop() tapi bisa kosong
// ════════════════════════════════════════════════════════════════
void loop() {
  vTaskDelay(portMAX_DELAY);  // yield selamanya, tidak buang CPU
}
