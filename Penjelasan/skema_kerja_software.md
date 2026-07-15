# Skema Kerja Software — Sistem IoT Pertanian Cerdas Cabai Rawit

---

## 1. Gambaran Umum Arsitektur

```
┌─────────────────────────────────────────────────────────────┐
│                        ESP32 (Dual Core)                    │
│                                                             │
│  ┌──────────────────────┐    ┌──────────────────────────┐   │
│  │   CORE 0             │    │   CORE 1                 │   │
│  │   taskKomunikasi     │    │   taskSensor             │   │
│  │   Prioritas: 1       │    │   Prioritas: 2           │   │
│  │   Stack: 10KB        │    │   Stack: 8KB             │   │
│  │                      │    │                          │   │
│  │  • WiFi reconnect    │    │  • Baca DHT22            │   │
│  │  • MQTT loop/pub     │    │  • Baca pH (blocking OK) │   │
│  │  • Supabase HTTP     │    │  • Baca Soil             │   │
│  │  • MQTT callback     │    │  • Fuzzy Tahani          │   │
│  │                      │    │  • Kontrol relay         │   │
│  └──────────┬───────────┘    └───────────┬──────────────┘   │
│             │                            │                   │
│             └──────── xMutex ────────────┘                   │
│                    SharedData (sd)                           │
│              flagDataBaru, flagRelayBerubah                  │
│              volatile ManualCmd                              │
└─────────────────────────────────────────────────────────────┘
         │                              │
    WiFi TLS 8883                  GPIO Relay
    MQTT + HTTPS                  GPIO ADC
         │                              │
   ┌─────┴──────┐              ┌────────┴───────┐
   │  EMQX Cloud │              │    Aktuator    │
   │  Supabase   │              │ Relay Kipas 25 │
   └─────────────┘              │ Relay Air   27 │
         │                      │ Relay pH    26 │
   Dashboard Web                └────────────────┘
```


---

## 2. Alur Inisialisasi (setup)

```
setup()
  │
  ├─ Serial.begin(115200)
  ├─ pinMode semua pin OUTPUT
  ├─ setRelay semua → OFF (aktif LOW, jadi HIGH = mati)
  ├─ digitalWrite DMS_PIN → HIGH (sensor pH nonaktif)
  ├─ dht.begin()
  ├─ xSemaphoreCreateMutex() → xMutex
  │    └─ GAGAL → ESP.restart()
  │
  ├─ xTaskCreatePinnedToCore(taskSensor,  Core 1, prioritas 2, 8KB)
  └─ xTaskCreatePinnedToCore(taskKomunikasi, Core 0, prioritas 1, 10KB)
       │
       └─ Kedua task berjalan paralel sejak sini
          loop() hanya memanggil vTaskDelay(portMAX_DELAY)
```

---

## 3. Alur taskKomunikasi (Core 0)

```
taskKomunikasi()
  │
  ├─ [INIT] WiFi.begin() — tunggu max 15 detik
  │    └─ GAGAL → ESP.restart()
  │
  ├─ [INIT] ntpSync() — sync waktu NTP pool.ntp.org (UTC+7)
  │
  ├─ [INIT] koneksiMQTT()
  │    ├─ tlsClient.setInsecure() (TLS tanpa verifikasi sertifikat)
  │    ├─ mqttClient.setServer(EMQX, 8883)
  │    ├─ mqttClient.setCallback(mqttCallback)
  │    └─ mqttClient.connect() + subscribe("pertanian/kontrol")
  │
  ├─ xTaskNotifyGive(hTaskSensor) ← beri sinyal taskSensor boleh mulai
  │
  └─ LOOP FOR(;;) setiap 10ms:
       │
       ├─ WiFi putus? → koneksiWiFi()
       │    └─ WiFi.begin() + vTaskDelay 500ms loop, max 15 detik
       │
       ├─ MQTT putus? → koneksiMQTT()
       │
       ├─ mqttClient.loop() ← HARUS dipanggil rutin (keep-alive + callback)
       │
       ├─ flagDataBaru == true?
       │    └─ publishMQTT() → kirim JSON ke "pertanian/sensor"
       │
       ├─ flagRelayBerubah == true?
       │    ├─ insertSupabase() → POST ke Supabase REST
       │    └─ reset lastDB (hindari double-kirim)
       │
       └─ (now - lastDB) >= 60 detik?
            └─ insertSupabase() → data periodik normal
```

---

## 4. MQTT Callback (dipanggil dari Core 0)

```
mqttCallback(topic, payload, length)
  │
  ├─ deserializeJson(payload) → gagal? return
  │
  ├─ Baca key "manual" → update manualCmd.aktif
  │
  ├─ Jika manual == true:
  │    ├─ Baca "kipas"     → manualCmd.kipas
  │    ├─ Baca "pompa_air" → manualCmd.pompa_air
  │    └─ Baca "pompa_ph"  → manualCmd.pompa_ph
  │
  └─ flagDataBaru = true
       └─ taskKomunikasi akan publishMQTT() di iterasi berikutnya
          sebagai feedback ke dashboard
```


---

## 5. Alur taskSensor (Core 1)

```
taskSensor()
  │
  ├─ ulTaskNotifyTake() ← BLOKIR sampai taskKomunikasi siap
  │
  └─ LOOP FOR(;;) setiap 20ms:
       │
       ├─ [A] FAST-PATH MANUAL (cek tiap 20ms)
       │
       └─ [B] SIKLUS SENSOR (cek tiap 15 detik)
            │
            ├─ tickPompa() ← timer relay independen
            └─ vTaskDelay(20ms)
```

### 5A. Fast-Path Manual (tiap 20ms)

```
Baca manualCmd (volatile — ditulis Core 0, dibaca Core 1)
  │
  ├─ Ada perubahan manual?
  │    (curManual ≠ prevManual ATAU nilai kipas/air/ph berubah)
  │
  ├─ YA — manualCmd.aktif == true:
  │    ├─ terapkanAktuator(kipas, air, ph, isManual=true)
  │    │    ├─ setRelay(RELAY_KIPAS, kipas) ← langsung
  │    │    ├─ manualPompaAir = pompa_air
  │    │    ├─ manualPompaPH  = pompa_ph
  │    │    └─ reqPompaAir = reqPompaPH = false (nonaktifkan AUTO)
  │    ├─ Update sd.relay_* via mutex
  │    └─ flagDataBaru = true
  │
  └─ YA — manualCmd.aktif == false (kembali AUTO):
       ├─ manualPompaAir = false
       ├─ manualPompaPH  = false
       ├─ setRelay(RELAY_KIPAS, false)
       └─ flagDataBaru = true
```

### 5B. Siklus Sensor Normal (tiap 15 detik)

```
1. DHT22
   └─ dht.readTemperature() + dht.readHumidity()

2. bacaPH(suhuLokal)
   └─ [lihat seksi 6 — Pipeline bacaPH]

3. Jeda isolasi galvanik 4000ms
   └─ loop 100ms × 40, tickPompa() setiap iterasi

4. bacaSoil()
   └─ [lihat seksi 7 — Pipeline bacaSoil]

5. Cross-contamination guard
   ├─ soil > 80% DAN |pH_baru - pH_lama| > 1.0 → pH = 0, reset
   └─ |pH_baru - pH_lama| > 1.5 tanpa syarat   → pH = 0, reset

6. inferensiFuzzy(suhu, soil, pH)
   └─ [lihat seksi 8 — Fuzzy Tahani]

7. terapkanAktuator(ki, ai, pi, isManual)
   └─ [lihat seksi 9 — Kontrol Aktuator]

8. Update SharedData via xSemaphoreTake(xMutex)
   └─ suhu, kelembaban, soil, pH, relay_*, mu_*, status_*, manual_mode

9. flagDataBaru = true → taskKomunikasi publishMQTT()

10. Log Serial
    └─ [HH:MM:SS] Suhu:X.XC Hum:X.X% SoilADC:XXXX Soil:X.X%
                  pHADC:XXXX pH:X.XX | Kipas:X Air:X pH:X [AUTO/MANUAL]
```


---

## 6. Pipeline bacaPH — Detail Tahap per Tahap

```
bacaPH(suhuLokal)
  │
  ├─ analogReadResolution(12) + analogSetAttenuation(ADC_11db)
  │
  ├─ [T1] DMS ON (GPIO13=LOW), LED ON
  │    └─ Loop 7000ms: tickPompa() setiap 100ms
  │
  ├─ [T2] Sampling 150 titik ADC
  │    └─ analogRead(GPIO34) × 150, jeda 5ms tiap sampel
  │         total waktu: ~750ms
  │
  ├─ [T3] DMS OFF (GPIO13=HIGH), LED OFF
  │    └─ Loop 2000ms: tickPompa() setiap 100ms
  │
  ├─ [T4] sortArray(s, 150) — insertion sort ascending
  │
  ├─ [T5] Validasi variansi
  │    ├─ varN = 150 × 5% = 7
  │    ├─ variansi = s[142] - s[7]
  │    └─ variansi > 150? → phResetState(), return 0.0f
  │
  ├─ [T6] Trimmed Mean 25%
  │    ├─ trimN = 150 × 25% = 37
  │    ├─ rata-rata s[37]–s[112] (76 sampel tengah)
  │    └─ adcFinal = adcTrim
  │
  ├─ [T7] Update sd.adcPH via mutex
  │
  ├─ [T8] Deteksi probe tidak terpasang
  │    ├─ adcFinal >= 1250?
  │    │    ├─ phNotTancapCnt++
  │    │    ├─ >= 5? → phResetState(), return 0.0f
  │    │    └─ < 5  → return phLast (nilai terakhir)
  │    └─ OK → phNotTancapCnt = 0
  │
  ├─ [T9] Deteksi drift naik ADC (elektroda mengering)
  │    ├─ adcFinal > adcPrevious + 8?
  │    │    ├─ phDriftCount++
  │    │    └─ >= 3? → phResetState(), adcPrevious=adcFinal, return 0.0f
  │    └─ OK → phDriftCount = 0, adcPrevious = adcFinal
  │
  ├─ [T10] Konversi ADC → pH
  │    ├─ adcToPH(adcFinal) — interpolasi piecewise 3 titik
  │    │    Segmen 1: ADC 520–689  → pH 9.18–6.86
  │    │    Segmen 2: ADC 689–1082 → pH 6.86–4.01
  │    │    Luar rentang: ekstrapolasi
  │    ├─ phRaw < 0 atau > 14? → phResetState(), return 0.0f
  │    └─ kompensasiSuhu(phRaw, suhu)
  │         phComp = phRaw - (0.003 × (suhu-25) × (phRaw-7))
  │
  ├─ [T11] Noise gate
  │    └─ |phComp - phLast| > 1.20? → phResetState(), return 0.0f
  │
  ├─ [T12] Warmup Kalman (3 siklus pertama)
  │    ├─ Siklus 1–2: kfEst=phComp, kfP=1.0, phLast=phComp, return 0.0f
  │    └─ Siklus 3  : kfEst=phComp, kfP=1.0, phLast=phComp, return phComp
  │
  ├─ [T13] Kalman Filter 1D
  │    ├─ kfP += Q (0.001)      ← Predict: ketidakpastian tumbuh
  │    ├─ K = kfP / (kfP + R)   ← Kalman Gain (R=0.40)
  │    │    K kecil (≈0.002) saat stabil → output halus
  │    │    K besar (≈0.7)   saat baru reset → cepat konvergen
  │    ├─ kfNew = kfEst + K × (phComp - kfEst)  ← Update
  │    ├─ kfP   = (1-K) × kfP
  │    └─ |kfNew - kfEst| >= 0.05?  ← Dead-band
  │         YA → kfEst = kfNew
  │         TIDAK → kfEst tetap (noise diabaikan)
  │
  ├─ [T14] Deteksi drift arah (elektroda belum ekuilibrasi)
  │    ├─ deltaKf = kfEst - phLast
  │    ├─ |deltaKf| >= 0.10?
  │    │    ├─ Searah dengan hitungan sebelumnya? → phDriftArahCnt ±= 1
  │    │    ├─ Arah berbalik? → reset phDriftArahCnt, catat phDriftArahRef
  │    │    └─ Pertama kali? → mulai hitung, catat phDriftArahRef
  │    └─ |deltaKf| < 0.10 → phDriftArahCnt = 0 (stabil)
  │
  ├─ [T15] Freeze output jika drift melewati batas
  │    ├─ |phDriftArahCnt| >= 7?
  │    │    └─ phOutput = phDriftArahRef (FREEZE)
  │    └─ Tidak? → phOutput = kfEst (normal)
  │
  ├─ phLast = phOutput
  └─ return phOutput
```


---

## 7. Pipeline bacaSoil — Detail Tahap per Tahap

```
bacaSoil()
  │
  ├─ analogReadResolution(12)
  │
  ├─ [T1] Sampling 50 titik ADC
  │    └─ analogRead(GPIO35) × 50, jeda 5ms tiap sampel
  │         total waktu: ~250ms
  │
  ├─ [T2] sortArray(s, 50)
  │
  ├─ [T3] Validasi variansi
  │    ├─ varN = 50 × 5% = 2
  │    ├─ variansi = s[47] - s[2]
  │    └─ variansi > 150?
  │         → soilKfEst=0, soilKfP=1, soilWarmup=true
  │         → settling delay 1500ms (dengan tickPompa tiap 100ms)
  │         → return 0.0f
  │
  ├─ [T4] Trimmed Mean 25%
  │    ├─ trimN = 50 × 25% = 12
  │    ├─ rata-rata s[12]–s[37] (26 sampel tengah)
  │    └─ raw = adcTrim
  │
  ├─ [T5] Update sd.adcSoil via mutex
  │
  ├─ [T6] Konversi ADC → persen
  │    ├─ map(raw, KERING=2900, BASAH=1139, 0, 100)
  │    │    ADC 2900 → 0%,  ADC 1139 → 100%
  │    │    (ADC besar = kering, ADC kecil = basah)
  │    └─ constrain(pct, 0, 100)
  │
  ├─ [T7] Deteksi sensor dicabut
  │    └─ raw > 2900 + 100 = 3000?
  │         → soilKfEst=0, soilKfP=1, soilWarmup=true
  │         → settling delay 1500ms
  │         → return 0.0f
  │
  ├─ [T8] Warmup Kalman (siklus pertama setelah reset)
  │    └─ soilWarmup == true?
  │         → soilKfEst=pct, soilKfP=1, soilWarmup=false
  │         → settling delay 1500ms
  │         → return soilKfEst (langsung output)
  │
  ├─ [T9] Reset Kalman jika perubahan besar
  │    └─ |pct - soilKfEst| > 25%?
  │         → soilKfEst=pct, soilKfP=1.0
  │            (konvergen cepat ke media baru)
  │
  ├─ [T10] Kalman Filter 1D Soil
  │    ├─ soilKfP += Q (0.0005)
  │    ├─ K = soilKfP / (soilKfP + R)  (R=2.00)
  │    │    K sangat kecil (≈0.0002) saat stabil
  │    │    → output bergerak sangat lambat
  │    ├─ soilNew = soilKfEst + K × (pct - soilKfEst)
  │    ├─ soilKfP = (1-K) × soilKfP
  │    └─ |soilNew - soilKfEst| >= 1.5%?  ← Dead-band
  │         YA → soilKfEst = soilNew
  │         TIDAK → soilKfEst tetap (penguapan < 1.5% diabaikan)
  │
  ├─ Settling delay 1500ms (dengan tickPompa tiap 100ms)
  └─ return soilKfEst
```

**Timing total bacaSoil:** ~2000ms (250ms sampling + 1500ms settling + overhead)

---

## 8. Inferensi Fuzzy Tahani

```
inferensiFuzzy(suhu, soil, pH)
  │
  ├─ Fuzzifikasi
  │    ├─ SUHU:
  │    │    fuzzyTempRendah(x): x≤24→1.0, 24–27→(27-x)/3, x≥27→0
  │    │    fuzzyTempSedang(x): x≤24→0, 24–27→(x-24)/3, 27–31→(31-x)/4, x≥31→0
  │    │    fuzzyTempTinggi(x): x≤27→0, 27–31→(x-27)/4, x≥31→1.0
  │    │
  │    ├─ SOIL (%):
  │    │    fuzzySoilKering(x): x≤40→1.0, 40–50→(50-x)/10, x≥50→0
  │    │    fuzzySoilLembab(x): x≤40→0, 40–50→(x-40)/10, 50–70→1.0, 70–80→(80-x)/10, x≥80→0
  │    │    fuzzySoilBasah(x):  x≤70→0, 70–80→(x-70)/10, x≥80→1.0
  │    │
  │    └─ pH:
  │         fuzzyPhAsam(x):   x≤5.0→1.0, 5–6→(6-x), x≥6→0
  │         fuzzyPhNormal(x): x≤5.5→0, 5.5–6→(x-5.5)/0.5, 6–7→1.0, 7–7.5→(7.5-x)/0.5, x≥7.5→0
  │         fuzzyPhBasa(x):   x≤7.0→0, 7–7.5→(x-7)/0.5, x≥7.5→1.0
  │
  ├─ Status label (untuk display):
  │    fuzzyTempStatus → "Rendah" / "Sedang" / "Tinggi"
  │    fuzzySoilStatus → "Kering" / "Lembab" / "Basah"
  │    fuzzyPhStatus   → "Asam" / "Normal" / "Basa"
  │
  ├─ Inferensi (Fuzzy Tahani — AND = min, aturan tunggal per aktuator):
  │    R1: suhu TINGGI   → Kipas ON
  │         mu_kipas     = fuzzyTempTinggi(suhu)
  │         kipas_on     = (mu_kipas > 0.5)
  │
  │    R2: tanah KERING  → Pompa Air ON
  │         mu_pompa_air = (soil <= 0) ? 0 : fuzzySoilKering(soil)
  │         pompa_air_on = (soil > 0 AND mu_pompa_air > 0.4)
  │
  │    R3: pH ASAM       → Pompa pH ON
  │         mu_pompa_ph  = (pH <= 0) ? 0 : fuzzyPhAsam(pH)
  │         pompa_ph_on  = (pH > 0 AND mu_pompa_ph > 0.4)
  │
  └─ return FuzzyOutput {kipas, pompa_air, pompa_ph, mu_*, status_*}
```


---

## 9. Kontrol Aktuator — tickPompa dan terapkanAktuator

### terapkanAktuator(kipas, pompa_air, pompa_ph, isManual)

```
isManual == true?
  ├─ setRelay(RELAY_KIPAS, kipas)    ← kipas langsung
  ├─ manualPompaAir = pompa_air      ← set flag manual
  ├─ manualPompaPH  = pompa_ph
  └─ reqPompaAir = reqPompaPH = false ← matikan AUTO

isManual == false?
  ├─ setRelay(RELAY_KIPAS, kipas)
  ├─ reqPompaAir    = pompa_air      ← set request AUTO
  ├─ reqPompaPH     = pompa_ph
  └─ manualPompaAir = manualPompaPH = false
```

### tickPompa() — dipanggil setiap 20ms

```
tickPompa()
  │
  ├─ ── POMPA AIR ──────────────────────────────────
  │
  ├─ manualPompaAir == true?
  │    ├─ pompaAirNyala == false?
  │    │    ├─ setRelay(RELAY_POMPA_AIR, ON)
  │    │    ├─ pompaAirNyala = true
  │    │    └─ flagRelayBerubah = true → Supabase segera
  │    └─ cdAir = -1 (nyala, tidak ada countdown)
  │
  ├─ manualPompaAir == false, pompaAirNyala == false:
  │    ├─ reqPompaAir == true?
  │    │    ├─ (now - pompaAirOffTime) >= 30 menit? (atau pertama kali)
  │    │    │    ├─ setRelay(RELAY_POMPA_AIR, ON)
  │    │    │    ├─ pompaAirNyala = true, catat pompaAirOnTime
  │    │    │    └─ flagRelayBerubah = true
  │    │    └─ Belum jeda? → cdAir = sisa detik jeda
  │    └─ reqPompaAir == false → setRelay(OFF), hitung cdAir
  │
  └─ pompaAirNyala == true (sedang nyala AUTO):
       ├─ cdAir = -1
       └─ (now - pompaAirOnTime) >= 10 detik?
            ├─ setRelay(RELAY_POMPA_AIR, OFF)
            ├─ pompaAirNyala = false
            ├─ pompaAirOffTime = now  ← mulai hitung jeda 30 menit
            ├─ reqPompaAir = false
            └─ flagRelayBerubah = true

  [Logika identik untuk POMPA pH dengan jeda 3 jam]

  ├─ Update sd.relay_pompa_air/ph = pompaAirNyala/pompaPHNyala via mutex
  └─ Update sd.countdown_air/ph
```

**Ringkasan perilaku:**

| Kondisi | Relay | Durasi | Jeda |
|---------|-------|--------|------|
| Manual ON | Hidup terus | Tidak terbatas | Tidak berlaku |
| Manual OFF | Mati | — | Jeda AUTO tidak berubah |
| AUTO — fuzzy aktif | Hidup | 10 detik | 30 menit (air) / 3 jam (pH) |
| AUTO — dalam jeda | Mati | — | countdown berjalan |
| AUTO — jeda habis | Siap nyala siklus berikutnya | — | — |

---

## 10. Sinkronisasi Antar Core

```
┌─────────────────────────────────────────────────────────────┐
│                    Mekanisme Sinkronisasi                   │
│                                                             │
│  xMutex (SemaphoreHandle_t)                                 │
│  ├─ Melindungi SharedData sd                                │
│  ├─ Diambil max 50ms sebelum timeout                        │
│  └─ Selalu dilepas setelah selesai                          │
│                                                             │
│  volatile ManualCmd manualCmd                               │
│  ├─ Ditulis Core 0 (mqttCallback)                           │
│  ├─ Dibaca Core 1 (taskSensor fast-path)                    │
│  └─ volatile ← cegah compiler cache register                │
│                                                             │
│  volatile bool flagDataBaru                                 │
│  ├─ Ditulis Core 1 (taskSensor)                             │
│  └─ Dibaca + reset Core 0 (taskKomunikasi)                  │
│                                                             │
│  volatile bool flagRelayBerubah                             │
│  ├─ Ditulis Core 1 (tickPompa)                              │
│  └─ Dibaca + reset Core 0 (taskKomunikasi)                  │
│                                                             │
│  xTaskNotifyGive / ulTaskNotifyTake                         │
│  ├─ Core 0 memberi notifikasi setelah WiFi+MQTT siap        │
│  └─ Core 1 blokir di awal sampai notifikasi diterima        │
└─────────────────────────────────────────────────────────────┘
```


---

## 11. Alur Pengiriman Data

### publishMQTT()
```
Ambil snapshot sd via mutex
  │
  └─ Kirim JSON ke topic "pertanian/sensor" (retained=true):
       temperature, humidity, soil, ph
       fuzzy_suhu, fuzzy_soil, fuzzy_ph
       relay_kipas, relay_air, relay_ph
       status_suhu, status_tanah, status_ph
       adc_ph, adc_soil, waktu
       manual_mode
       countdown_air, countdown_ph  (-1=nyala, 0=siap, >0=detik jeda)
       wifi_ssid, wifi_ip, wifi_rssi, wifi_status
```

### insertSupabase()
```
Ambil snapshot sd via mutex
  │
  └─ HTTP POST ke Supabase REST API:
       https://<project>.supabase.co/rest/v1/pertanian
       Headers: apikey, Authorization Bearer, Prefer: return=minimal
       Body JSON: temperature, humidity, soil, ph,
                  fuzzy_suhu, fuzzy_soil, fuzzy_ph,
                  relay_kipas, relay_air, relay_ph,
                  updated_at (ISO 8601 UTC+7)
```

### Kapan Supabase dipanggil:
```
1. flagRelayBerubah == true  ← SEGERA saat relay berubah state
   (pompa nyala atau mati, manual atau auto)

2. Setiap 60 detik           ← data periodik normal
   (suhu, kelembaban, sensor, dll)
```

---

## 12. Timing Keseluruhan Satu Siklus Sensor

```
Waktu (detik)  Kegiatan
──────────────────────────────────────────────────────
0.00           Siklus sensor dimulai
0.00           DHT22 readTemperature + readHumidity (~0.25 detik)
0.25           bacaPH() dimulai
0.25             DMS ON + tunggu stabil 7 detik
7.25             Sampling 150 × 5ms = 0.75 detik
8.00             DMS OFF + jeda 2 detik
10.00          bacaPH() selesai, return pH
10.00          Jeda isolasi galvanik 4 detik
14.00          bacaSoil() dimulai
14.00            Sampling 50 × 5ms = 0.25 detik
14.25            Kalman + settling delay 1.5 detik
15.75          bacaSoil() selesai, return soil
15.75          Cross-contamination guard (~0ms)
15.75          inferensiFuzzy (~0ms)
15.75          terapkanAktuator (~0ms)
15.75          Update SharedData + publishMQTT
15.75          Tunggu siklus berikutnya (INTERVAL_SENSOR_MS=15 detik)

Total durasi aktif per siklus: ~15.75 detik
Interval antar siklus: 15 detik (overlap jika durasi > interval)
```

**Catatan:** `tickPompa()` tetap berjalan setiap 20ms selama seluruh
siklus karena semua delay menggunakan loop dengan `tickPompa()` di dalamnya.

---

## 13. Diagram Alur Keputusan Relay (Mode AUTO)

```
Setiap siklus sensor (±15 detik):

SUHU > 27°C?  (mu_tinggi > 0.5)
     │
     ├─ YA  → setRelay(KIPAS, ON)   langsung, tanpa timer/jeda
     └─ TIDAK → setRelay(KIPAS, OFF)

SOIL < 50%?  (mu_kering > 0.4) DAN soil > 0?
     │
     ├─ YA  → reqPompaAir = true
     │          tickPompa() cek:
     │            Dalam jeda 30 menit? → tunggu
     │            Jeda habis? → relay ON 10 detik → jeda 30 menit
     └─ TIDAK → reqPompaAir = false → relay OFF

pH < 6.0?  (mu_asam > 0.4) DAN pH > 0?
     │
     ├─ YA  → reqPompaPH = true
     │          tickPompa() cek:
     │            Dalam jeda 3 jam? → tunggu
     │            Jeda habis? → relay ON 10 detik → jeda 3 jam
     └─ TIDAK → reqPompaPH = false → relay OFF
```

---

## 14. Diagram Alur Keputusan Relay (Mode MANUAL)

```
Perintah MQTT masuk → mqttCallback() → manualCmd diupdate
     │
     └─ fast-path (tiap 20ms) deteksi perubahan:
          │
          ├─ manual = true, kipas = X
          │    → setRelay(RELAY_KIPAS, X) LANGSUNG
          │
          ├─ manual = true, pompa_air = true
          │    → manualPompaAir = true
          │    → tickPompa(): relay ON terus (tidak ada timer durasi)
          │    → jeda AUTO TIDAK berubah
          │
          ├─ manual = true, pompa_air = false
          │    → manualPompaAir = false
          │    → tickPompa(): relay OFF
          │
          └─ manual = false (kembali AUTO)
               → manualPompaAir = manualPompaPH = false
               → setRelay(KIPAS, OFF)
               → fuzzy ambil alih di siklus berikutnya
```

---

## 15. Ringkasan Waktu Respons

| Aksi | Waktu Respons |
|------|---------------|
| Perintah MQTT masuk → Serial log | < 50ms (MQTT loop 10ms) |
| Perintah MQTT → kipas nyala | < 50ms (setRelay langsung di fast-path) |
| Perintah MQTT → pompa nyala | < 100ms (tickPompa tiap 20ms) |
| Perubahan relay → data Supabase tercatat | < 200ms (flagRelayBerubah) |
| Sensor berubah → MQTT publish | < 50ms setelah siklus sensor selesai |
| Sensor berubah → Supabase | 60 detik (periodik) atau saat relay berubah |
| Power on → output pH pertama | ~50 detik (3 warmup × 15 detik + overhead) |
| Power on → Kalman pH stabil | ~3 menit |
| Power on → output Soil pertama | ~17 detik (1 warmup siklus) |

---

*Dokumen ini menggambarkan skema kerja software sistem IoT Pertanian Cerdas
Cabai Rawit berbasis ESP32 FreeRTOS dengan arsitektur dual-core.*
