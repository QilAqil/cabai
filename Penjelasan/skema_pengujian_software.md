# Skema Pengujian Software — Sistem IoT Pertanian Cerdas Cabai Rawit

---

## Diagram 1: Skema Utama Alur Sistem (Mengacu Gambar)

```mermaid
flowchart TD
    START([Start]) --> S1 & S2 & S3

    S1[Sensor DHT22\nGPIO 4]
    S2[Sensor Soil Moisture\nGPIO 35]
    S3[Sensor pH Tanah\nGPIO 34 + DMS GPIO 13]

    S1 --> N1[/Nilai Suhu °C\nNilai Kelembaban %/]
    S2 --> N2[/Nilai Kelembaban\nTanah %/]
    S3 --> N3[/Nilai pH\n0.00 – 14.00/]

    N1 & N2 & N3 --> FUZZ[Pengolahan Data\nFuzzy Tahani di ESP32]

    FUZZ --> KL[/Kualitas Lingkungan\nStatus Suhu · Tanah · pH/]
    KL --> AK[Kendali Aktuator\nKipas · Pompa Air · Pompa pH]

    FUZZ --> KIRIM[Proses Pengiriman\nke Database]
    AK --> KIRIM

    KIRIM --> DB[(Database\nSupabase PostgreSQL)]

    DB --> DASH[Menampilkan Data Sensor\ndi Dashboard Web]
    DB --> RIWAYAT[Riwayat Kendali\nAktuator]
```


---

## Diagram 2: Alur Lengkap dengan Nilai Nyata — Pengujian Kasus 1
### pH Asam (pH = 5.2) → Pompa pH Menyala

```mermaid
flowchart TD
    START([Start]) --> D1 & D2 & D3

    subgraph DHT["Sensor DHT22"]
        D1[Sensor DHT22\nGPIO 4] --> N1[/Suhu = 26°C\nKelembaban = 65%/]
    end

    subgraph SOIL["Sensor Soil Moisture"]
        D2[Sensor Soil\nGPIO 35\n50 sampel ADC] --> N2[/ADC = 2100\nKelembaban Tanah = 46%/]
    end

    subgraph PH["Sensor pH Tanah"]
        D3[Sensor pH\nGPIO 34\nDMS GPIO 13 ON\n150 sampel ADC] --> N3[/ADC = 950\npH = 4.98/]
    end

    N1 & N2 & N3 --> FUZZ

    subgraph FUZZ["Pengolahan Data Fuzzy Tahani"]
        F1["Fuzzifikasi Suhu = 26°C
        µ Rendah = 0.33
        µ Sedang = 0.67
        µ Tinggi = 0.00"] 
        F2["Fuzzifikasi Soil = 46%
        µ Kering = 0.40
        µ Lembab = 0.60
        µ Basah  = 0.00"]
        F3["Fuzzifikasi pH = 4.98
        µ Asam   = 1.00
        µ Normal = 0.00
        µ Basa   = 0.00"]

        F1 --> R1{"R1: µ Tinggi = 0.00\n≤ 0.5 ?"}
        F2 --> R2{"R2: µ Kering = 0.40\n≤ 0.4 ?"}
        F3 --> R3{"R3: µ Asam = 1.00\n> 0.4 ?"}

        R1 -->|YA| K_OFF[Kipas = OFF]
        R2 -->|YA tepat sama| A_OFF[Pompa Air = OFF]
        R3 -->|YA| P_ON[Pompa pH = ON]
    end

    K_OFF & A_OFF & P_ON --> KL[/Kualitas Lingkungan
    Suhu   : Sedang ✓
    Tanah  : Lembab ✓
    pH     : Asam ✗/]

    KL --> AK["Kendali Aktuator
    Kipas     : OFF
    Pompa Air : OFF
    Pompa pH  : ON → GPIO26 LOW"]

    AK --> KIRIM[Proses Pengiriman ke Database\nMQTT + Supabase]
    KIRIM --> DB[(Database Supabase\nrelay_ph = 1\nph = 4.98\nupdated_at = timestamp)]
    DB --> DASH[Dashboard Web\nmenampilkan pH = 4.98\nPompa pH = ON]

    style P_ON fill:#ff6b6b,color:#fff
    style AK fill:#ff6b6b,color:#fff
    style K_OFF fill:#51cf66,color:#fff
    style A_OFF fill:#51cf66,color:#fff
```


---

## Diagram 3: Pengujian Kasus 2
### Tanah Kering (Soil = 35%) → Pompa Air Menyala

```mermaid
flowchart TD
    START([Start]) --> D1 & D2 & D3

    D1[Sensor DHT22\nGPIO 4] --> N1[/Suhu = 27°C\nKelembaban = 70%/]
    D2[Sensor Soil\nGPIO 35] --> N2[/ADC = 2700\nKelembaban Tanah = 11%/]
    D3[Sensor pH\nGPIO 34] --> N3[/ADC = 689\npH = 6.86/]

    N1 & N2 & N3 --> FUZZ

    subgraph FUZZ["Pengolahan Data Fuzzy Tahani"]
        F1["Fuzzifikasi Suhu = 27°C
        µ Tinggi = 0.00 — batas bawah"]
        F2["Fuzzifikasi Soil = 11%
        µ Kering = 1.00 — x ≤ 40"]
        F3["Fuzzifikasi pH = 6.86
        µ Asam = 0.00 — x > 6.0"]

        F1 --> R1{"µ Tinggi = 0.00\n≤ 0.5"}
        F2 --> R2{"µ Kering = 1.00\n> 0.4"}
        F3 --> R3{"µ Asam = 0.00\n≤ 0.4"}

        R1 -->|TIDAK nyala| K_OFF[Kipas = OFF]
        R2 -->|NYALA| A_ON[Pompa Air = ON]
        R3 -->|TIDAK nyala| P_OFF[Pompa pH = OFF]
    end

    A_ON --> CEKJEDA{"tickPompa:\nJeda 30 menit\nsudah habis?"}
    CEKJEDA -->|YA atau pertama kali| RELAY_ON["Relay Pompa Air ON\nGPIO27 LOW\nHitung OnTime = now"]
    CEKJEDA -->|TIDAK masih dalam jeda| TUNGGU["Relay TIDAK nyala\nCountdown = sisa detik\nTampil di dashboard"]

    RELAY_ON --> DURASI["Tunggu 10 detik"] --> RELAY_OFF["Relay Pompa Air OFF\nGPIO27 HIGH\nOffTime = now\nMulai jeda 30 menit"]

    K_OFF & RELAY_OFF & P_OFF --> KL[/Kualitas Lingkungan
    Suhu  : Sedang ✓
    Tanah : Kering ✗
    pH    : Normal ✓/]

    KL --> KIRIM[Proses Pengiriman ke Database]
    KIRIM --> DB[(Database Supabase\nrelay_air = 1 saat nyala\nrelay_air = 0 saat mati\nsoil = 11%)]
    DB --> DASH[Dashboard Web\nSoil = 11%\nPompa Air = ON → OFF\nCountdown = 1800 detik]

    style A_ON fill:#ff6b6b,color:#fff
    style RELAY_ON fill:#ff6b6b,color:#fff
    style RELAY_OFF fill:#51cf66,color:#fff
    style TUNGGU fill:#868e96,color:#fff
```


---

## Diagram 4: Pengujian Kasus 3
### Suhu Tinggi (Suhu = 32°C) → Kipas Menyala

```mermaid
flowchart TD
    START([Start]) --> D1 & D2 & D3

    D1[Sensor DHT22\nGPIO 4] --> N1[/Suhu = 32°C\nKelembaban = 55%/]
    D2[Sensor Soil\nGPIO 35] --> N2[/ADC = 2050\nKelembaban Tanah = 48%/]
    D3[Sensor pH\nGPIO 34] --> N3[/ADC = 700\npH = 6.77/]

    N1 & N2 & N3 --> FUZZ

    subgraph FUZZ["Pengolahan Data Fuzzy Tahani"]
        F1["Fuzzifikasi Suhu = 32°C
        µ Tinggi = 1.00 — x ≥ 31"]
        F2["Fuzzifikasi Soil = 48%
        µ Kering = 0.20 — (50-48)/10"]
        F3["Fuzzifikasi pH = 6.77
        µ Asam = 0.00 — x > 6.0"]

        F1 --> R1{"µ Tinggi = 1.00\n> 0.5"}
        F2 --> R2{"µ Kering = 0.20\n≤ 0.4"}
        F3 --> R3{"µ Asam = 0.00\n≤ 0.4"}

        R1 -->|NYALA| K_ON[Kipas = ON]
        R2 -->|TIDAK nyala| A_OFF[Pompa Air = OFF]
        R3 -->|TIDAK nyala| P_OFF[Pompa pH = OFF]
    end

    K_ON --> RELAY_K["setRelay GPIO25 LOW
    Kipas ON LANGSUNG
    Tanpa timer · Tanpa jeda
    Hidup selama µ Tinggi > 0.5"]

    K_ON & A_OFF & P_OFF --> KL[/Kualitas Lingkungan
    Suhu  : Tinggi ✗
    Tanah : Lembab ✓
    pH    : Normal ✓/]

    KL --> KIRIM[Proses Pengiriman ke Database]
    KIRIM --> DB[(Database Supabase\nrelay_kipas = 1\ntemperature = 32\nupdated_at = timestamp)]
    DB --> DASH[Dashboard Web\nSuhu = 32°C\nKipas = ON]

    RELAY_K --> SIKLUS["Siklus berikutnya 15 detik\nSuhu = 26°C\nµ Tinggi = 0.00 ≤ 0.5"]
    SIKLUS --> K_OFF["setRelay GPIO25 HIGH\nKipas OFF LANGSUNG"]

    style K_ON fill:#ff6b6b,color:#fff
    style RELAY_K fill:#ff6b6b,color:#fff
    style K_OFF fill:#51cf66,color:#fff
```


---

## Diagram 5: Pengujian Kasus 4
### Semua Kondisi Buruk → Semua Aktuator Menyala Bersamaan

```mermaid
flowchart TD
    START([Start]) --> D1 & D2 & D3

    D1[Sensor DHT22\nGPIO 4] --> N1[/Suhu = 33°C\nKelembaban = 40%/]
    D2[Sensor Soil\nGPIO 35] --> N2[/ADC = 2750\nKelembaban Tanah = 9%/]
    D3[Sensor pH\nGPIO 34] --> N3[/ADC = 1000\npH = 4.53/]

    N1 & N2 & N3 --> FUZZ

    subgraph FUZZ["Pengolahan Data Fuzzy Tahani"]
        F1["µ Tinggi Suhu = 1.00\n33°C ≥ 31 → penuh"]
        F2["µ Kering Soil = 1.00\n9% ≤ 40 → penuh"]
        F3["µ Asam pH  = 1.00\n4.53 ≤ 5.0 → penuh"]

        F1 --> R1{"µ = 1.00 > 0.5"} -->|NYALA| K_ON
        F2 --> R2{"µ = 1.00 > 0.4"} -->|NYALA| A_ON
        F3 --> R3{"µ = 1.00 > 0.4"} -->|NYALA| P_ON

        K_ON[Kipas = ON]
        A_ON[Pompa Air = ON]
        P_ON[Pompa pH = ON]
    end

    K_ON --> RK["GPIO25 LOW\nKipas ON langsung"]
    A_ON --> RA["GPIO27 LOW\nPompa Air ON\n10 detik → OFF\njeda 30 menit"]
    P_ON --> RP["GPIO26 LOW\nPompa pH ON\n10 detik → OFF\njeda 3 jam"]

    RK & RA & RP --> KL[/Kualitas Lingkungan
    Suhu  : Tinggi  ✗
    Tanah : Kering  ✗
    pH    : Asam    ✗/]

    KL --> KIRIM[Proses Pengiriman ke Database\nflagRelayBerubah = true]
    KIRIM --> DB[(Database Supabase\nrelay_kipas = 1\nrelay_air = 1\nrelay_ph = 1\nDikirim SEGERA)]
    DB --> DASH[Dashboard Web\nSemua indikator MERAH\nKipas ON · Pompa Air ON · Pompa pH ON]

    style K_ON fill:#ff6b6b,color:#fff
    style A_ON fill:#ff6b6b,color:#fff
    style P_ON fill:#ff6b6b,color:#fff
    style RK fill:#ff6b6b,color:#fff
    style RA fill:#ff6b6b,color:#fff
    style RP fill:#ff6b6b,color:#fff
```


---

## Diagram 6: Pengujian Kasus 5
### Kondisi Normal → Semua Aktuator OFF

```mermaid
flowchart TD
    START([Start]) --> D1 & D2 & D3

    D1[Sensor DHT22\nGPIO 4] --> N1[/Suhu = 26°C\nKelembaban = 72%/]
    D2[Sensor Soil\nGPIO 35] --> N2[/ADC = 1800\nKelembaban Tanah = 62%/]
    D3[Sensor pH\nGPIO 34] --> N3[/ADC = 700\npH = 6.77/]

    N1 & N2 & N3 --> FUZZ

    subgraph FUZZ["Pengolahan Data Fuzzy Tahani"]
        F1["Fuzzifikasi Suhu = 26°C
        µ Tinggi = 0.00 — x < 27"]
        F2["Fuzzifikasi Soil = 62%
        µ Kering = 0.00 — x > 50"]
        F3["Fuzzifikasi pH = 6.77
        µ Asam = 0.00 — x > 6.0"]

        F1 --> R1{"µ Tinggi = 0.00\n≤ 0.5"} -->|TIDAK| K_OFF[Kipas = OFF]
        F2 --> R2{"µ Kering = 0.00\n≤ 0.4"} -->|TIDAK| A_OFF[Pompa Air = OFF]
        F3 --> R3{"µ Asam = 0.00\n≤ 0.4"} -->|TIDAK| P_OFF[Pompa pH = OFF]
    end

    K_OFF & A_OFF & P_OFF --> KL[/Kualitas Lingkungan
    Suhu  : Sedang ✓ OPTIMAL
    Tanah : Lembab ✓ OPTIMAL
    pH    : Normal ✓ OPTIMAL/]

    KL --> AK["Kendali Aktuator
    Semua relay HIGH = OFF
    Sistem dalam kondisi ideal"]

    AK --> KIRIM[Proses Pengiriman ke Database\nData sensor tiap 15 detik via MQTT\nSupabase update tiap 60 detik]
    KIRIM --> DB[(Database Supabase\nrelay_kipas = 0\nrelay_air = 0\nrelay_ph = 0)]
    DB --> DASH[Dashboard Web\nSemua indikator HIJAU\nTidak ada aksi diperlukan]

    style K_OFF fill:#51cf66,color:#fff
    style A_OFF fill:#51cf66,color:#fff
    style P_OFF fill:#51cf66,color:#fff
    style KL fill:#51cf66,color:#fff
    style AK fill:#51cf66,color:#fff
```


---

## Diagram 7: Pengujian Kontrol Manual via Dashboard

```mermaid
flowchart TD
    START([Start]) --> DASH[Dashboard Web\nOperator tekan tombol\nPompa Air = ON Manual]

    DASH --> MQTT_PUB["Kirim MQTT ke ESP32
    Topic: pertanian/kontrol
    Payload:
    { manual: true,
      pompa_air: true,
      kipas: false,
      pompa_ph: false }"]

    MQTT_PUB --> CB["ESP32 mqttCallback Core 0
    manualCmd.aktif     = true
    manualCmd.pompa_air = true
    manualCmd.kipas     = false
    manualCmd.pompa_ph  = false
    flagDataBaru = true"]

    CB --> FP["taskSensor Core 1
    fast-path deteksi perubahan
    setiap 20ms"]

    FP --> TA["terapkanAktuator
    isManual = true
    setRelay KIPAS → OFF
    manualPompaAir = true
    reqPompaAir = false"]

    TA --> TICK["tickPompa 20ms
    manualPompaAir == true
    pompaAirNyala == false?"]

    TICK --> RON["setRelay GPIO27 LOW
    Pompa Air ON
    pompaAirNyala = true
    flagRelayBerubah = true"]

    RON --> SB["insertSupabase SEGERA
    relay_air = 1
    manual_mode = true"]

    RON --> HIDUP["Pompa Air HIDUP TERUS
    selama manualCmd.pompa_air = true
    TIDAK ada timer 10 detik
    TIDAK ada jeda 30 menit"]

    HIDUP --> OFF_CMD[Operator tekan\nPompa Air = OFF Manual]
    OFF_CMD --> CB2["MQTT:
    { manual: true, pompa_air: false }"]
    CB2 --> FP2["fast-path:
    manualPompaAir = false"]
    FP2 --> ROFF["tickPompa:
    manualPompaAir = false
    setRelay GPIO27 HIGH
    Pompa Air OFF"]

    ROFF --> NOTE["Jeda AUTO 30 menit
    TIDAK berubah
    Pompa AUTO tetap berjalan
    sesuai jadwal semula"]

    style RON fill:#ff6b6b,color:#fff
    style HIDUP fill:#ff6b6b,color:#fff
    style ROFF fill:#51cf66,color:#fff
    style NOTE fill:#74c0fc,color:#000
```


---

## Diagram 8: Pengujian Data ke Database dan Dashboard

```mermaid
flowchart TD
    ESP["ESP32 — selesai satu siklus sensor
    Suhu=29°C · Soil=38% · pH=5.3
    Kipas=OFF · Pompa Air=ON · Pompa pH=ON"]

    ESP --> MQTT_PUB["publishMQTT
    Topic: pertanian/sensor
    JSON lengkap 24 field"]

    ESP --> FLAG{flagRelayBerubah\n== true?}
    FLAG -->|YA relay baru berubah| SB_NOW["insertSupabase SEGERA
    POST /rest/v1/pertanian
    relay_air=1 · relay_ph=1
    Dikirim < 200ms"]

    FLAG -->|TIDAK| TIMER{"60 detik\nhabis?"}
    TIMER -->|YA| SB_60["insertSupabase Periodik
    Data sensor terkini"]
    TIMER -->|TIDAK| SKIP["Lewati\ntunggu timer"]

    MQTT_PUB --> EMQX["EMQX Cloud
    Topic retained
    Broker simpan nilai terakhir"]

    EMQX --> DASH["Dashboard Web
    Tampil real-time:
    • Suhu = 29°C 🌡
    • Soil = 38% 💧
    • pH = 5.3 🧪
    • Kipas = OFF
    • Pompa Air = ON ●
    • Pompa pH = ON ●
    • Countdown Air = -1 nyala
    • Countdown pH = -1 nyala"]

    SB_NOW --> SB_DB[("Supabase PostgreSQL
    Baris baru tersimpan:
    temperature = 29
    soil = 38
    ph = 5.3
    relay_kipas = 0
    relay_air = 1
    relay_ph = 1
    updated_at = 2025-07-15T10:23:45+07:00")]

    SB_DB --> DASH2["Dashboard Web
    Tabel riwayat data
    Graf sensor historis
    Status relay per menit"]

    style SB_NOW fill:#ff6b6b,color:#fff
    style SB_DB fill:#4dabf7,color:#fff
    style DASH fill:#51cf66,color:#fff
```


---

## Tabel Ringkasan Hasil Pengujian

| Kasus | Suhu (°C) | Soil (%) | pH | Kipas | Pompa Air | Pompa pH |
|-------|-----------|----------|----|-------|-----------|----------|
| 1 — pH Asam | 26 | 60 | **4.98** | OFF | OFF | **ON** |
| 2 — Tanah Kering | 27 | **11** | 6.86 | OFF | **ON** | OFF |
| 3 — Suhu Tinggi | **32** | 48 | 6.77 | **ON** | OFF | OFF |
| 4 — Semua Buruk | **33** | **9** | **4.53** | **ON** | **ON** | **ON** |
| 5 — Normal | 26 | 62 | 6.77 | OFF | OFF | OFF |
| 6 — Manual | bebas | bebas | bebas | sesuai perintah | **ON terus** | sesuai perintah |

**Batas nilai untuk setiap aktuator:**

```mermaid
graph LR
    subgraph KIPAS["Kipas Menyala"]
        K1["Suhu > 28.5°C\nµ_tinggi > 0.5"]
    end
    subgraph AIR["Pompa Air Menyala"]
        A1["Soil < 46%\nµ_kering > 0.4"]
    end
    subgraph PH["Pompa pH Menyala"]
        P1["pH < 5.6\nµ_asam > 0.4"]
    end

    K1 --> K2["GPIO25 LOW\nON langsung\nOFF langsung"]
    A1 --> A2["GPIO27 LOW\nON 10 detik\nJeda 30 menit"]
    P1 --> P2["GPIO26 LOW\nON 10 detik\nJeda 3 jam"]
```

---

*Skema pengujian ini menggambarkan alur sistem dari pembacaan sensor,*
*pengolahan Fuzzy Tahani, kendali aktuator, hingga penyimpanan ke database.*
