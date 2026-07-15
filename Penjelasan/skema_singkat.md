# Skema Sistem — Tiga Parameter ke Aktuator

```mermaid
flowchart TD
    START([Start])

    START --> DHT[Sensor DHT22\nSuhu & Kelembaban]
    START --> SOIL[Sensor Soil Moisture\nKelembaban Tanah]
    START --> PH[Sensor pH Tanah\npH Larutan]

    DHT --> N1[/Suhu = 32°C/]
    SOIL --> N2[/Soil = 35%/]
    PH --> N3[/pH = 4.98/]

    N1 --> F1{"Fuzzy Suhu\nµ Tinggi = 1.00\n> 0.5 ?"}
    N2 --> F2{"Fuzzy Soil\nµ Kering = 1.00\n> 0.4 ?"}
    N3 --> F3{"Fuzzy pH\nµ Asam = 1.00\n> 0.4 ?"}

    F1 -->|YA| K[Kipas ON\nGPIO25]
    F1 -->|TIDAK| KO[Kipas OFF]

    F2 -->|YA| A[Pompa Air ON\nGPIO27\n10 detik → jeda 30 menit]
    F2 -->|TIDAK| AO[Pompa Air OFF]

    F3 -->|YA| P[Pompa pH ON\nGPIO26\n10 detik → jeda 3 jam]
    F3 -->|TIDAK| PO[Pompa pH OFF]

    K & A & P & KO & AO & PO --> DB[(Supabase\nDatabase)]
    DB --> DASH[Dashboard Web\nTampil data real-time]

    style K fill:#ff6b6b,color:#fff
    style A fill:#ff6b6b,color:#fff
    style P fill:#ff6b6b,color:#fff
    style KO fill:#51cf66,color:#fff
    style AO fill:#51cf66,color:#fff
    style PO fill:#51cf66,color:#fff
```

---

## Batas Nilai Aktuator Menyala

| Parameter | Nilai | Aktuator | Durasi |
|-----------|-------|----------|--------|
| Suhu > **28.5°C** | µ_tinggi > 0.5 | Kipas ON | Selama kondisi terpenuhi |
| Soil < **46%** | µ_kering > 0.4 | Pompa Air ON | 10 detik, jeda 30 menit |
| pH < **5.6** | µ_asam > 0.4 | Pompa pH ON | 10 detik, jeda 3 jam |
