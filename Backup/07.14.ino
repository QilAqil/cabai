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
const int SOIL_ADC_KERING = 2900;   // ADC sensor di udara/dicabut ≈ 4095, tanah kering ≈ 2800-2900
const int SOIL_ADC_BASAH  = 1139;   // ADC sensor dalam air nyata (terukur Serial)

const TickType_t INTERVAL_SENSOR_MS = 15000;  // siklus sensor (ms)
const TickType_t INTERVAL_DB_MS     = 60000;  // siklus Supabase (ms)

// Anti-interferensi soil ↔ pH
const unsigned long SOIL_SETTLE_MS    = 1500;
const int           SOIL_SAMPLES      = 50;
const int           SOIL_VARIANSI_MAX = 150;

// Jeda isolasi galvanik: setelah bacaSoil selesai, tunggu sebelum mulai bacaPH
// agar muatan kapasitif dari probe soil habis terlebih dahulu
// Dinaikkan 3000→4000ms: interferensi masih terdeteksi dengan 3 detik
const unsigned long GALVANIC_ISOLASI_MS = 4000;

const unsigned long INTERVAL_PH_ON  = 7000;
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
const float PH_NOISE_GATE   = 1.20f;   // dikecilkan 2.00→1.20: jeda isolasi 3 detik sudah mengurangi galvanik
const float PH_TEMP_COEF    = 0.003f;
const float PH_TEMP_REF     = 25.0f;
const int   PH_ADC_BATAS    = 1250;
const int   PH_NOTANCAP_CNT = 5;
const int   PH_VARIANSI_MAX = 150;     // dikecilkan 250→150: selaraskan dengan threshold soil
const int   PH_WARMUP_N     = 3;
const int   PH_DRIFT_MIN    = 8;
const int   PH_DRIFT_CNT_MAX= 3;

// ── Kalman Filter 1D — parameter ─────────────────────────────
const float PH_KALMAN_Q     = 0.001f;
const float PH_KALMAN_R     = 0.40f;   // dinaikkan 0.30→0.40: lebih skeptis lagi
const float PH_DEADBAND     = 0.05f;   // output pH tidak bergerak jika perubahan < 0.05 unit

// Deteksi drift arah lambat
// MIN dinaikkan: perubahan < 0.10/siklus dianggap noise, bukan drift nyata
// MAX dinaikkan 5→7: beri lebih banyak siklus sebelum freeze, hindari false-freeze
const int   PH_DRIFT_ARAH_MAX  = 7;    // freeze setelah 7 siklus searah (~105 detik)
const float PH_DRIFT_ARAH_MIN  = 0.10f; // dinaikkan 0.05→0.10: hanya hitung drift jika perubahan nyata

// ── Kalman Filter 1D untuk Soil — parameter ──────────────────
const float SOIL_KALMAN_Q     = 0.0005f; // diturunkan 0.001→0.0005: lebih percaya estimasi sendiri
const float SOIL_KALMAN_R     = 2.00f;   // dinaikkan 1.50→2.00: lebih skeptis lagi terhadap ADC
const float SOIL_DEADBAND     = 1.5f;    // output tidak bergerak jika perubahan < 1.5% (noise penguapan)

// ════════════════════════════════════════════════════════════════
//  OBJEK & RTOS HANDLE
// ════════════════════════════════════════════════════════════════
DHT              dht(DHTPIN, DHTTYPE);
WiFiClientSecure tlsClient;
PubSubClient     mqttClient(tlsClient);

// Mutex — melindungi akses ke shared data antara taskSensor dan taskKomunikasi
SemaphoreHandle_t xMutex = NULL;

// Notifikasi: taskSensor memberi tahu taskKomunikasi ada data baru
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
//  FILTER pH — STATE KALMAN 1D (hanya diakses dari taskSensor / Core 1)
//
//  Model:   x̂ₖ = x̂ₖ₋₁  (pH dianggap konstan antar siklus)
//  Update:  K  = P / (P + R)
//           x̂  = x̂ + K * (z - x̂)
//           P  = (1 - K) * P + Q
//
//  Variabel:
//    kfEst  — estimasi pH terkini
//    kfP    — error covariance (ukuran ketidakpastian estimasi)
// ════════════════════════════════════════════════════════════════
static float kfEst           = 0.0f;   // estimasi Kalman pH
static float kfP             = 1.0f;   // error covariance pH
static float phLast          = 0.0f;   // output siklus sebelumnya (untuk noise gate)
static int   phNotTancapCnt  = 0;
static int   phWarmupCount   = 0;
static int   adcPrevious     = 0;
static int   phDriftCount    = 0;
// Deteksi drift arah — hitung berapa siklus pH bergerak searah
static int   phDriftArahCnt  = 0;      // >0 = drift turun, <0 = drift naik
static float phDriftArahRef  = 0.0f;  // nilai referensi awal drift

static void phResetState() {
  kfEst = phLast = 0.0f;
  kfP   = 1.0f;
  phWarmupCount  = 0;
  phDriftCount   = 0;
  phDriftArahCnt = 0;
  phDriftArahRef = 0.0f;
  adcPrevious    = 0;    // reset agar drift-naik check tidak salah banding nilai lama
}

// ── State Kalman Soil — hanya diakses dari taskSensor (Core 1) ──
static float soilKfEst       = 0.0f;   // estimasi Kalman soil (%)
static float soilKfP         = 1.0f;   // error covariance soil
static bool  soilWarmup      = true;   // true = siklus pertama, seed langsung ke nilai terukur

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

  // DMS ON — tunggu elektroda stabilisasi, diselingi tickPompa tiap 100ms
  // agar perintah manual relay tetap bisa dieksekusi selama menunggu
  digitalWrite(DMS_PIN, LOW);
  digitalWrite(LED_PIN, HIGH);
  for (unsigned long elapsed = 0; elapsed < INTERVAL_PH_ON; elapsed += 100) {
    tickPompa();
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  // 150 sampel
  int s[PH_SAMPLES];
  for (int i = 0; i < PH_SAMPLES; i++) {
    s[i] = analogRead(PH_ADC_PIN);
    vTaskDelay(pdMS_TO_TICKS(5));
  }

  digitalWrite(DMS_PIN, HIGH);
  digitalWrite(LED_PIN, LOW);
  // Delay OFF juga diselingi tickPompa
  for (unsigned long elapsed = 0; elapsed < INTERVAL_PH_OFF; elapsed += 100) {
    tickPompa();
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  sortArray(s, PH_SAMPLES);

  // Cek variansi (5% trimmed)
  int varN     = PH_SAMPLES * 5 / 100;
  int variansi = s[PH_SAMPLES-1-varN] - s[varN];
  if (variansi > PH_VARIANSI_MAX) {
    Serial.printf("[pH] Variansi=%d → tidak stabil\n", variansi);
    phResetState(); return 0.0f;
  }

  // Spike check: jika variansi rendah, adcTrim sudah representatif
  int trimN = PH_SAMPLES * PH_TRIM_PCT / 100;
  long sumT = 0; int cntT = 0;
  for (int i = trimN; i < PH_SAMPLES-trimN; i++) { sumT += s[i]; cntT++; }
  int adcTrim  = (int)(sumT / cntT);
  int adcFinal = adcTrim;

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

  // Noise gate — tolak lonjakan besar yang tidak mungkin terjadi secara nyata
  if (phLast > 0.0f && fabsf(phComp - phLast) > PH_NOISE_GATE) {
    Serial.printf("[pH] Noise gate delta=%.3f → reset\n", fabsf(phComp-phLast));
    phResetState(); return 0.0f;
  }

  // Warmup — konvergenkan Kalman sebelum output dipakai
  // Siklus 1–2: seed dan buang (elektroda belum stabil)
  // Siklus 3 (terakhir): seed terakhir, langsung return nilai
  if (phWarmupCount < PH_WARMUP_N) {
    phWarmupCount++;
    kfEst  = phComp;
    kfP    = 1.0f;
    phLast = phComp;
    Serial.printf("[pH] Warmup %d/%d ADC=%d phRaw=%.3f phComp=%.3f\n",
      phWarmupCount, PH_WARMUP_N, adcFinal, phRaw, phComp);
    if (phWarmupCount < PH_WARMUP_N) return 0.0f;  // buang siklus 1–2
    // Siklus warmup terakhir: langsung output nilai awal
    return phComp;
  }

  // ── Kalman Filter 1D ─────────────────────────────────────────
  kfP += PH_KALMAN_Q;
  float K   = kfP / (kfP + PH_KALMAN_R);
  float kfNew = kfEst + K * (phComp - kfEst);
  kfP = (1.0f - K) * kfP;

  // Dead-band: abaikan perubahan < 0.05 unit (noise ADC residual)
  if (fabsf(kfNew - kfEst) >= PH_DEADBAND) {
    kfEst = kfNew;
  }

  // ── Deteksi drift arah lambat (drift elektroda belum ekuilibrasi) ──────
  // Jika output Kalman bergerak searah terus > PH_DRIFT_ARAH_MAX siklus
  // dengan perubahan minimal PH_DRIFT_ARAH_MIN per siklus → freeze output
  float deltaKf = kfEst - phLast;
  if (phLast > 0.0f && fabsf(deltaKf) >= PH_DRIFT_ARAH_MIN) {
    bool turun = (deltaKf < 0.0f);
    if (phDriftArahCnt == 0) {
      // Mulai hitung drift arah baru
      phDriftArahCnt = turun ? 1 : -1;
      phDriftArahRef = phLast;
    } else if ((phDriftArahCnt > 0) == turun) {
      // Masih searah — tambah hitungan
      phDriftArahCnt += turun ? 1 : -1;
    } else {
      // Arah berbalik — reset hitungan
      phDriftArahCnt = turun ? 1 : -1;
      phDriftArahRef = phLast;
    }
  } else {
    // Perubahan sangat kecil — reset hitungan, elektroda stabil
    phDriftArahCnt = 0;
  }

  // Freeze output jika drift searah sudah melewati batas
  float phOutput = kfEst;
  if (abs(phDriftArahCnt) >= PH_DRIFT_ARAH_MAX) {
    phOutput = phDriftArahRef;  // pakai nilai referensi awal drift
    // Jangan reset Kalman — biarkan internal terus update,
    // kalau drift berbalik arah, phDriftArahCnt akan reset sendiri
    Serial.printf("[pH] Drift arah %s %d siklus → freeze di %.3f\n",
      phDriftArahCnt > 0 ? "turun" : "naik", abs(phDriftArahCnt), phOutput);
  }

  phLast = phOutput;

  Serial.printf("[pH] ADCtrim=%d var=%d phRaw=%.3f phComp=%.3f K=%.3f kfEst=%.3f phOut=%.3f driftCnt=%d\n",
    adcTrim, variansi, phRaw, phComp, K, kfEst, phOutput, phDriftArahCnt);
  return phOutput;
}

// ════════════════════════════════════════════════════════════════
//  BACA SOIL — dijalankan di Core 1, dengan settling delay + Kalman
// ════════════════════════════════════════════════════════════════
float bacaSoil() {
  analogReadResolution(12);

  // Ambil 50 sampel dengan jarak 5ms → total 250ms
  int s[SOIL_SAMPLES];
  for (int i = 0; i < SOIL_SAMPLES; i++) {
    s[i] = analogRead(SOIL_PIN);
    vTaskDelay(pdMS_TO_TICKS(5));
  }

  // Sort untuk trimmed mean + cek variansi
  sortArray(s, SOIL_SAMPLES);

  // Cek variansi (5% trimmed) — tolak jika ADC terlalu berisik
  // (misal saat pompa air baru mati, tanah masih meneteskan air)
  int varN        = SOIL_SAMPLES * 5 / 100;
  int variansi    = s[SOIL_SAMPLES-1-varN] - s[varN];
  if (variansi > SOIL_VARIANSI_MAX) {
    Serial.printf("[Soil] Variansi=%d → tidak stabil, pakai nilai lama\n", variansi);
    for (unsigned long elapsed = 0; elapsed < SOIL_SETTLE_MS; elapsed += 100) {
      tickPompa();
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    return soilKfEst > 0.0f ? soilKfEst : 0.0f;
  }

  // Trimmed mean 25% — buang 25% atas dan bawah
  int trimN = SOIL_SAMPLES * 25 / 100;
  long sumT = 0; int cntT = 0;
  for (int i = trimN; i < SOIL_SAMPLES - trimN; i++) { sumT += s[i]; cntT++; }
  int raw = (int)(sumT / cntT);

  // Update shared ADC
  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    sd.adcSoil = raw;
    xSemaphoreGive(xMutex);
  }

  // Konversi ADC → persen kelembaban
  float pct = (float)map(raw, SOIL_ADC_KERING, SOIL_ADC_BASAH, 0, 100);
  pct = constrain(pct, 0.0f, 100.0f);

  // ── Deteksi sensor dicabut: ADC mendekati maksimum (udara = ~4095) ────
  // Threshold: ADC > KERING+100 dianggap sensor tidak terpasang
  if (raw > SOIL_ADC_KERING + 100) {
    Serial.printf("[Soil] Sensor tidak terpasang (ADC=%d)\n", raw);
    soilKfEst  = 0.0f;
    soilKfP    = 1.0f;
    soilWarmup = true;   // paksa warmup ulang saat sensor ditancap kembali
    for (unsigned long elapsed = 0; elapsed < SOIL_SETTLE_MS; elapsed += 100) {
      tickPompa();
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    return 0.0f;
  }

  // ── Kalman Filter 1D ─────────────────────────────────────────
  if (soilWarmup) {
    soilKfEst = pct;
    soilKfP   = 1.0f;
    soilWarmup = false;
    Serial.printf("[Soil] Warmup ADC=%d → %.1f%% var=%d\n", raw, pct, variansi);
    for (unsigned long elapsed = 0; elapsed < SOIL_SETTLE_MS; elapsed += 100) {
      tickPompa();
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    return soilKfEst;
  }

  // Reset Kalman jika perubahan mendadak > 25% (misal sensor baru ditancap ke media berbeda)
  // Ini agar output langsung mengejar nilai nyata, tidak merangkak lambat
  if (fabsf(pct - soilKfEst) > 25.0f) {
    Serial.printf("[Soil] Perubahan besar %.1f%%→%.1f%% → reset Kalman\n", soilKfEst, pct);
    soilKfEst = pct;
    soilKfP   = 1.0f;   // reset P agar K besar di siklus berikutnya
  }

  soilKfP  += SOIL_KALMAN_Q;
  float K   = soilKfP / (soilKfP + SOIL_KALMAN_R);
  float soilNew = soilKfEst + K * (pct - soilKfEst);
  soilKfP   = (1.0f - K) * soilKfP;

  // Dead-band: abaikan perubahan sangat kecil (noise penguapan gradual)
  if (fabsf(soilNew - soilKfEst) >= SOIL_DEADBAND) {
    soilKfEst = soilNew;
  }

  Serial.printf("[Soil] ADC=%d raw=%.1f%% K=%.4f kfOut=%.1f%% var=%d\n",
    raw, pct, K, soilKfEst, variansi);

  // Settling delay diselingi tickPompa agar relay manual tetap responsif
  for (unsigned long elapsed = 0; elapsed < SOIL_SETTLE_MS; elapsed += 100) {
    tickPompa();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  return soilKfEst;
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
// Permintaan dari fuzzy (AUTO) — tunduk pada jeda
static bool reqPompaAir = false;
static bool reqPompaPH  = false;
// Permintaan dari manual — bypass jeda, langsung nyala
static bool manualPompaAir = false;
static bool manualPompaPH  = false;

// ── tickPompa: dipanggil setiap ~20ms dari taskSensor loop ──────
//
// DUA JALUR TERPISAH:
//   AUTO   : reqPompaAir / reqPompaPH   → nyala 5 detik, tunduk jeda
//   MANUAL : manualPompaAir / manualPompaPH → hidup TERUS selama flag aktif
//
// Jeda AUTO tidak berubah saat manual dipakai.
void tickPompa() {
  unsigned long now = millis();
  int cdAir = 0, cdPH = 0;

  // ── Pompa Air ────────────────────────────────────────────────
  if (manualPompaAir) {
    // MANUAL: hidup terus, tidak ada timer durasi
    setRelay(RELAY_POMPA_AIR, true);
    pompaAirNyala = true;
    cdAir = -1;
  } else if (!pompaAirNyala) {
    // Tidak manual, tidak nyala — cek request AUTO
    if (reqPompaAir) {
      if (pompaAirOffTime == 0 || now - pompaAirOffTime >= POMPA_AIR_JEDA) {
        setRelay(RELAY_POMPA_AIR, true);
        pompaAirNyala  = true;
        pompaAirOnTime = now;
        Serial.println("[PompaAir] ON (AUTO) — nyala 5 detik");
      } else {
        unsigned long sisa = POMPA_AIR_JEDA - (now - pompaAirOffTime);
        cdAir = (int)(sisa / 1000);
      }
    } else {
      setRelay(RELAY_POMPA_AIR, false);
      if (pompaAirOffTime > 0 && now - pompaAirOffTime < POMPA_AIR_JEDA)
        cdAir = (int)((POMPA_AIR_JEDA - (now - pompaAirOffTime)) / 1000);
    }
  } else {
    // Sedang nyala AUTO — cek durasi 5 detik
    cdAir = -1;
    if (now - pompaAirOnTime >= POMPA_AIR_DURASI) {
      setRelay(RELAY_POMPA_AIR, false);
      pompaAirNyala   = false;
      pompaAirOffTime = now;
      reqPompaAir     = false;
      Serial.printf("[PompaAir] OFF (AUTO) — jeda 30 menit\n");
      cdAir = (int)(POMPA_AIR_JEDA / 1000);
    }
  }

  // ── Pompa pH ─────────────────────────────────────────────────
  if (manualPompaPH) {
    // MANUAL: hidup terus, tidak ada timer durasi
    setRelay(RELAY_POMPA_PH, true);
    pompaPHNyala = true;
    cdPH = -1;
  } else if (!pompaPHNyala) {
    if (reqPompaPH) {
      if (pompaPHOffTime == 0 || now - pompaPHOffTime >= POMPA_PH_JEDA) {
        setRelay(RELAY_POMPA_PH, true);
        pompaPHNyala  = true;
        pompaPHOnTime = now;
        Serial.println("[PompaPH] ON (AUTO) — nyala 5 detik");
      } else {
        unsigned long sisa = POMPA_PH_JEDA - (now - pompaPHOffTime);
        cdPH = (int)(sisa / 1000);
      }
    } else {
      setRelay(RELAY_POMPA_PH, false);
      if (pompaPHOffTime > 0 && now - pompaPHOffTime < POMPA_PH_JEDA)
        cdPH = (int)((POMPA_PH_JEDA - (now - pompaPHOffTime)) / 1000);
    }
  } else {
    // Sedang nyala AUTO — cek durasi 5 detik
    cdPH = -1;
    if (now - pompaPHOnTime >= POMPA_PH_DURASI) {
      setRelay(RELAY_POMPA_PH, false);
      pompaPHNyala   = false;
      pompaPHOffTime = now;
      reqPompaPH     = false;
      Serial.printf("[PompaPH] OFF (AUTO) — jeda 3 jam\n");
      cdPH = (int)(POMPA_PH_JEDA / 1000);
    }
  }

  // Log sisa jeda tiap ~60 detik
  if (cdAir > 0) {
    static unsigned long lastLogAir = 0;
    if (now - lastLogAir >= 60000UL) {
      lastLogAir = now;
      int m = cdAir / 60, s = cdAir % 60;
      Serial.printf("[PompaAir] Jeda AUTO: %02d:%02d (sisa %d detik)\n", m, s, cdAir);
    }
  }
  if (cdPH > 0) {
    static unsigned long lastLogPH = 0;
    if (now - lastLogPH >= 60000UL) {
      lastLogPH = now;
      int h = cdPH / 3600, m = (cdPH % 3600) / 60, s = cdPH % 60;
      Serial.printf("[PompaPH]  Jeda AUTO: %02d:%02d:%02d (sisa %d detik)\n", h, m, s, cdPH);
    }
  }

  // Update countdown ke shared data
  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    sd.countdown_air = cdAir;
    sd.countdown_ph  = cdPH;
    xSemaphoreGive(xMutex);
  }
}

// ── terapkanAktuator: set permintaan relay berdasarkan mode ──────
// Kipas langsung dikontrol di sini.
// Pompa AUTO  → reqPompaAir / reqPompaPH  (tunduk jeda)
// Pompa MANUAL → manualPompaAir / manualPompaPH (bypass jeda)
void terapkanAktuator(bool kipas, bool pompa_air, bool pompa_ph, bool isManual) {
  setRelay(RELAY_KIPAS, kipas);
  if (isManual) {
    manualPompaAir = pompa_air;
    manualPompaPH  = pompa_ph;
    // Pastikan req AUTO tidak ikut aktif saat manual
    reqPompaAir    = false;
    reqPompaPH     = false;
  } else {
    reqPompaAir    = pompa_air;
    reqPompaPH     = pompa_ph;
    // Pastikan flag manual tidak aktif saat AUTO
    manualPompaAir = false;
    manualPompaPH  = false;
  }
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
//  KONEKSI WiFi — hanya untuk reconnect di dalam FreeRTOS task
// ════════════════════════════════════════════════════════════════
void koneksiWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-t < 15000)
    vTaskDelay(pdMS_TO_TICKS(500));
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
        terapkanAktuator(curKipas, curAir, curPH, true);
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
        // Reset flag manual pompa agar tickPompa tidak terus nyalakan pompa
        manualPompaAir = false;
        manualPompaPH  = false;
        setRelay(RELAY_KIPAS, false);  // kipas ikut mati, fuzzy yang akan nyalakan lagi
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

      // ── Jeda isolasi galvanik ─────────────────────────────────
      // Probe soil meninggalkan muatan kapasitif di medium setelah
      // pengukuran selesai. Jeda ini memberi waktu muatan tersebut
      // terurai sebelum elektroda pH mulai sampling, sehingga
      // potensial yang dibaca pH tidak tergeser oleh arus sisa soil.
      Serial.printf("[Isolasi] Jeda galvanik %lums sebelum baca pH...\n", GALVANIC_ISOLASI_MS);
      for (unsigned long elapsed = 0; elapsed < GALVANIC_ISOLASI_MS; elapsed += 100) {
        tickPompa();
        vTaskDelay(pdMS_TO_TICKS(100));
      }

      // 3. pH — ambil suhu lokal dulu, lalu baca (boleh blocking)
      float suhuLokal = (!isnan(t)) ? t : 0.0f;
      float phBaru    = bacaPH(suhuLokal);

      // ── Cross-contamination guard (diperkuat) ─────────────────
      // Tolak pembacaan pH jika:
      //   a) soil sangat basah (>80%) DAN pH lompat >1.0 → interferensi galvanik masih ada
      //   b) pH lompat >1.5 tanpa syarat soil → spike ADC
      float phSekarang;
      if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        phSekarang = sd.pH;
        xSemaphoreGive(xMutex);
      } else { phSekarang = 0.0f; }

      if (phSekarang > 0.0f) {
        float deltaPH = fabsf(phBaru - phSekarang);
        if (soilVal > 80.0f && deltaPH > 1.0f) {
          Serial.printf("[pH] Cross-guard (galvanik) soil=%.1f%% delta=%.3f → reset 0\n",
            soilVal, deltaPH);
          phBaru = 0.0f;
          phResetState();
        } else if (deltaPH > 1.5f) {
          Serial.printf("[pH] Cross-guard (spike) delta=%.3f → reset 0\n", deltaPH);
          phBaru = 0.0f;
          phResetState();
        }
      }

      // 4. Fuzzy inferensi
      FuzzyOutput fo = inferensiFuzzy(suhuLokal, soilVal, phBaru);

      // 5. Terapkan aktuator — mode manual sudah ditangani fast-path di atas,
      //    di sini hanya terapkan jika mode otomatis
      bool isManual = manualCmd.aktif;
      bool ki = isManual ? manualCmd.kipas     : fo.kipas;
      bool ai = isManual ? manualCmd.pompa_air : fo.pompa_air;
      bool pi = isManual ? manualCmd.pompa_ph  : fo.pompa_ph;
      terapkanAktuator(ki, ai, pi, isManual);

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

    tickPompa();                    // eksekusi relay DULU di iterasi ini
    vTaskDelay(pdMS_TO_TICKS(20)); // turunkan 50→20ms: fast-path lebih responsif
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
    NULL,
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