# Alur Keputusan Aktuator — Contoh Nyata End-to-End

---

## Contoh 1: pH Asam → Pompa pH Menyala

**Kondisi sensor:** pH = 5.2, Soil = 60%, Suhu = 26°C

```mermaid
flowchart TD
    A["Sensor pH membaca ADC = 950
    Trimmed Mean 150 sampel"] --> B

    B["adcToPH: ADC 950
    Segmen ADC 689–1082 → pH 6.86–4.01
    pH = 6.86 + (950-689)/(1082-689) × (4.01-6.86)
    pH = 6.86 + 0.664 × (-2.85)
    phRaw = 4.97"] --> C

    C["Kompensasi suhu 26°C
    phComp = 4.97 - (0.003 × 1 × -2.03)
    phComp = 4.97 + 0.006 = 4.976 ≈ 4.98"] --> D

    D["Kalman Filter
    kfEst = 4.98 → phOutput = 4.98"] --> E

    E["Fuzzifikasi pH = 4.98
    fuzzyPhAsam: x=4.98 < 5.0 → µ = 1.0
    fuzzyPhNormal: x=4.98 < 5.5 → µ = 0.0
    fuzzyPhBasa: x=4.98 < 7.0 → µ = 0.0"] --> F

    F{"R3: µ_asam = 1.0
    > threshold 0.4
    DAN pH > 0?"} -->|YA| G

    G["pompa_ph = true
    reqPompaPH = true"] --> H

    H{"tickPompa:
    pompaPHOffTime == 0
    atau jeda 3 jam habis?"} -->|YA pertama kali| I

    I["setRelay GPIO26 LOW
    Relay Pompa pH ON
    pompaPHNyala = true
    flagRelayBerubah = true"] --> J

    J["insertSupabase SEGERA
    relay_ph = 1"] --> K

    K["10 detik berlalu"] --> L

    L["setRelay GPIO26 HIGH
    Relay Pompa pH OFF
    pompaPHOffTime = now
    Mulai hitung jeda 3 jam
    flagRelayBerubah = true"] --> M

    M["insertSupabase SEGERA
    relay_ph = 0"]

    style I fill:#ff6b6b,color:#fff
    style L fill:#51cf66,color:#fff
    style G fill:#ffd43b
```


---

## Contoh 2: Tanah Kering → Pompa Air Menyala

**Kondisi sensor:** Soil = 35%, pH = 6.5, Suhu = 26°C

```mermaid
flowchart TD
    A["Sensor Soil membaca ADC = 2600
    Trimmed Mean 50 sampel"] --> B

    B["map ADC ke persen:
    map(2600, KERING=2900, BASAH=1139, 0, 100)
    pct = (2900-2600)/(2900-1139) × 100
    pct = 300/1761 × 100 = 17%
    
    Kalman: soilKfEst = 35%
    (akumulasi beberapa siklus)"] --> C

    C["Fuzzifikasi Soil = 35%
    fuzzySoilKering: x=35 ≤ 40 → µ = 1.0
    fuzzySoilLembab: x=35 ≤ 40 → µ = 0.0
    fuzzySoilBasah:  x=35 ≤ 70 → µ = 0.0"] --> D

    D{"R2: µ_kering = 1.0
    > threshold 0.4
    DAN soil > 0?"} -->|YA| E

    E["pompa_air = true
    reqPompaAir = true"] --> F

    F{"tickPompa:
    pompaAirOffTime == 0
    atau jeda 30 menit habis?"} -->|YA pertama kali| G

    G["setRelay GPIO27 LOW
    Relay Pompa Air ON
    pompaAirNyala = true
    pompaAirOnTime = now
    flagRelayBerubah = true"] --> H

    H["insertSupabase SEGERA
    relay_air = 1"] --> I

    I["10 detik berlalu"] --> J

    J["setRelay GPIO27 HIGH
    Relay Pompa Air OFF
    pompaAirOffTime = now
    Mulai hitung jeda 30 menit
    flagRelayBerubah = true"] --> K

    K["insertSupabase SEGERA
    relay_air = 0"] --> L

    L{"Siklus berikutnya
    soil masih kering
    DAN belum 30 menit?"} -->|YA| M

    M["reqPompaAir = true
    TAPI tickPompa cek:
    now - pompaAirOffTime < 30 menit"] --> N

    N["cdAir = sisa detik
    Relay TIDAK nyala
    Tunggu jeda habis"]

    style G fill:#ff6b6b,color:#fff
    style J fill:#51cf66,color:#fff
    style E fill:#ffd43b
    style N fill:#868e96,color:#fff
```


---

## Contoh 3: Suhu Tinggi → Kipas Menyala

**Kondisi sensor:** Suhu = 32°C, Soil = 65%, pH = 6.5

```mermaid
flowchart TD
    A["DHT22 membaca
    Suhu = 32°C"] --> B

    B["Fuzzifikasi Suhu = 32°C
    fuzzyTempRendah: x=32 > 27 → µ = 0.0
    fuzzyTempSedang: x=32 ≥ 31 → µ = 0.0
    fuzzyTempTinggi: x=32 ≥ 31 → µ = 1.0"] --> C

    C{"R1: µ_tinggi = 1.0
    > threshold 0.5?"} -->|YA| D

    D["kipas = true"] --> E

    E["terapkanAktuator:
    setRelay GPIO25 LOW
    Relay Kipas ON LANGSUNG
    tidak ada timer durasi
    tidak ada jeda"] --> F

    F["Kipas HIDUP TERUS
    selama suhu > 27°C"] --> G

    G{"Siklus berikutnya
    Suhu = 26°C
    µ_tinggi = 0?"} -->|µ = 0, tidak > 0.5| H

    H["kipas = false
    setRelay GPIO25 HIGH
    Relay Kipas OFF LANGSUNG"]

    style E fill:#ff6b6b,color:#fff
    style H fill:#51cf66,color:#fff
    style D fill:#ffd43b
```


---

## Contoh 4: Kondisi Normal → Semua Aktuator OFF

**Kondisi sensor:** Suhu = 26°C, Soil = 65%, pH = 6.5

```mermaid
flowchart TD
    A["Sensor membaca:
    Suhu = 26°C · Soil = 65% · pH = 6.5"] --> B

    B["Fuzzifikasi Suhu = 26°C
    fuzzyTempTinggi: x=26, 27-31 → µ = 0.0
    µ_tinggi = 0.0 ≤ 0.5 → Kipas OFF"] --> C

    C["Fuzzifikasi Soil = 65%
    fuzzySoilKering: x=65 > 50 → µ = 0.0
    µ_kering = 0.0 ≤ 0.4 → Pompa Air OFF"] --> D

    D["Fuzzifikasi pH = 6.5
    fuzzyPhAsam: x=6.5 > 6.0 → µ = 0.0
    µ_asam = 0.0 ≤ 0.4 → Pompa pH OFF"] --> E

    E["Semua aktuator OFF
    Sistem dalam kondisi optimal
    Tidak ada tindakan"] --> F

    F["Data tetap dikirim tiap 15 detik
    via MQTT ke dashboard
    Supabase update tiap 60 detik"]

    style E fill:#51cf66,color:#fff
```


---

## Contoh 5: pH di Zona Transisi (Sebagian Asam)

**Kondisi sensor:** pH = 5.6 (antara Asam dan Normal)

```mermaid
flowchart TD
    A["pH terukur = 5.6"] --> B

    B["Fuzzifikasi pH = 5.6
    fuzzyPhAsam:   5 < x=5.6 < 6 → µ = 6.0-5.6 = 0.4
    fuzzyPhNormal: 5.5 < x=5.6 < 6 → µ = (5.6-5.5)/0.5 = 0.2
    fuzzyPhBasa:   x=5.6 < 7.0 → µ = 0.0"] --> C

    C{"R3: µ_asam = 0.4
    > threshold 0.4?"} -->|TIDAK persis sama| D

    D["Threshold 0.4 bersifat STRICT greater than
    pH 5.6 → µ = 0.4 tepat sama
    pompa_ph = FALSE
    Pompa pH TIDAK nyala"] --> E

    E["Jika pH = 5.5:
    fuzzyPhAsam: 5 < 5.5 < 6 → µ = 6.0-5.5 = 0.5
    µ = 0.5 > 0.4 → Pompa pH NYALA"] --> F

    F["Kesimpulan:
    pH ≤ 5.5 → Pompa pH ON
    pH 5.5-6.0 → zona transisi, tergantung nilai tepat
    pH ≥ 6.0 → Pompa pH OFF"]

    style D fill:#ffd43b
    style F fill:#74c0fc
```


---

## Ringkasan Tabel Keputusan Semua Aktuator

```mermaid
flowchart LR
    subgraph INPUT["Nilai Sensor"]
        S1["Suhu = 32°C"]
        S2["Suhu = 29°C\nzona transisi"]
        S3["Suhu = 26°C"]
        S4["Soil = 35%"]
        S5["Soil = 45%\nzona transisi"]
        S6["Soil = 65%"]
        S7["pH = 4.5"]
        S8["pH = 5.5\nbatas bawah"]
        S9["pH = 6.5"]
    end

    subgraph FUZZY["Nilai µ dan Keputusan"]
        F1["µ_tinggi = 1.0\n> 0.5 → KIPAS ON"]
        F2["µ_tinggi = 0.5\n= 0.5 → KIPAS OFF\ntidak strict greater"]
        F3["µ_tinggi = 0.0\n≤ 0.5 → KIPAS OFF"]
        F4["µ_kering = 1.0\n> 0.4 → POMPA AIR ON"]
        F5["µ_kering = 0.5\n> 0.4 → POMPA AIR ON"]
        F6["µ_kering = 0.0\n≤ 0.4 → POMPA AIR OFF"]
        F7["µ_asam = 1.0\n> 0.4 → POMPA pH ON"]
        F8["µ_asam = 0.5\n> 0.4 → POMPA pH ON"]
        F9["µ_asam = 0.0\n≤ 0.4 → POMPA pH OFF"]
    end

    subgraph OUTPUT["Aktuator"]
        O1["Kipas ON\nGPIO25 LOW\ntanpa timer"]
        O2["Kipas OFF\nGPIO25 HIGH"]
        O3["Pompa Air ON\nGPIO27 LOW\n10 detik\njeda 30 menit"]
        O4["Pompa Air OFF\nGPIO27 HIGH"]
        O5["Pompa pH ON\nGPIO26 LOW\n10 detik\njeda 3 jam"]
        O6["Pompa pH OFF\nGPIO26 HIGH"]
    end

    S1 --> F1 --> O1
    S2 --> F2 --> O2
    S3 --> F3 --> O2
    S4 --> F4 --> O3
    S5 --> F5 --> O3
    S6 --> F6 --> O4
    S7 --> F7 --> O5
    S8 --> F8 --> O5
    S9 --> F9 --> O6
```


---

## Batas Nilai Sensor untuk Setiap Aktuator

| Sensor | Nilai | µ | Aktuator | Status |
|--------|-------|---|----------|--------|
| Suhu | < 27°C | µ_tinggi = 0 | Kipas | OFF |
| Suhu | 27–31°C | µ_tinggi = 0–1 | Kipas | OFF jika µ ≤ 0.5 |
| Suhu | **> 28.5°C** | µ_tinggi **> 0.5** | Kipas | **ON** |
| Suhu | ≥ 31°C | µ_tinggi = 1.0 | Kipas | ON penuh |
| Soil | > 50% | µ_kering = 0 | Pompa Air | OFF |
| Soil | 40–50% | µ_kering = 0–1 | Pompa Air | OFF jika µ ≤ 0.4 |
| Soil | **< 46%** | µ_kering **> 0.4** | Pompa Air | **ON** |
| Soil | ≤ 40% | µ_kering = 1.0 | Pompa Air | ON penuh |
| pH | > 6.0 | µ_asam = 0 | Pompa pH | OFF |
| pH | 5.0–6.0 | µ_asam = 0–1 | Pompa pH | OFF jika µ ≤ 0.4 |
| pH | **< 5.6** | µ_asam **> 0.4** | Pompa pH | **ON** |
| pH | ≤ 5.0 | µ_asam = 1.0 | Pompa pH | ON penuh |

> **Batas kritis:**
> - Kipas menyala saat suhu **> 28.5°C**
> - Pompa Air menyala saat kelembaban tanah **< 46%**
> - Pompa pH menyala saat pH **< 5.6**

---

*File ini fokus pada logika keputusan aktuator dengan contoh nilai nyata.*
*Untuk pipeline sensor lengkap lihat: `penjelasan_sensor_ph.md`*
*Untuk diagram sistem lengkap lihat: `diagram_sistem.md`*
