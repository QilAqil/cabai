-- =============================================================================
-- Skema tabel `pertanian` — referensi lengkap (proyek baru / from scratch).
-- Punya tabel lama (fuzzy_air, tanpa fuzzy_soil)? Jalankan:
--   supabase/migration_sync_pertanian_for_esp32.sql
-- ESP32 mengirim: fuzzy_suhu (suhu→paranet), fuzzy_soil (tanah→air), fuzzy_ph, tiga relay.
-- =============================================================================

-- Tabel baru (jika belum ada)
CREATE TABLE IF NOT EXISTS public.pertanian (
  id              BIGSERIAL PRIMARY KEY,
  created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),

  -- Sensor
  temperature     REAL NOT NULL DEFAULT 0,    -- suhu udara (°C), DHT11
  humidity        REAL NOT NULL DEFAULT 0,    -- kelembaban udara (%), DHT11
  soil            INTEGER NOT NULL DEFAULT 0, -- kelembaban tanah (0–100%)
  ph              REAL NOT NULL DEFAULT 0,    -- pH tanah

  -- Skor indikator 0–1 (dashboard / analisis)
  fuzzy_suhu      REAL NOT NULL DEFAULT 0,    -- skor fuzzy suhu udara → paranet
  fuzzy_soil      REAL NOT NULL DEFAULT 0,   -- skor kebutuhan penyiraman (dari kelembaban tanah)
  fuzzy_ph        REAL NOT NULL DEFAULT 0,   -- skor kebutuhan koreksi pH (dari pH)

  -- Relay: 0 = OFF, 1 = ON (ESP32 mengirim angka)
  relay_paranet   SMALLINT NOT NULL DEFAULT 0, -- paranet (suhu tinggi)
  relay_air       SMALLINT NOT NULL DEFAULT 0, -- pompa penyiraman air (tanah kering)
  relay_dolomit   SMALLINT NOT NULL DEFAULT 0  -- relay koreksi pH / larutan (nama kolom legacy)
);

COMMENT ON TABLE public.pertanian IS 'Log sensor & kontrol IoT pertanian (MQTT + Supabase)';
COMMENT ON COLUMN public.pertanian.fuzzy_suhu IS 'Skor fuzzy suhu udara (paranet), 0–1';
COMMENT ON COLUMN public.pertanian.fuzzy_soil IS 'Skor 0–1 kebutuhan air (kelembaban tanah)';
COMMENT ON COLUMN public.pertanian.fuzzy_ph IS 'Skor 0–1 koreksi pH';
COMMENT ON COLUMN public.pertanian.relay_dolomit IS 'Status relay koreksi pH (legacy: dolomit)';

-- =============================================================================
-- Migrasi dari tabel lama: tambah kolom yang belum ada (aman dijalankan ulang)
-- =============================================================================
ALTER TABLE public.pertanian ADD COLUMN IF NOT EXISTS fuzzy_soil REAL NOT NULL DEFAULT 0;
ALTER TABLE public.pertanian ADD COLUMN IF NOT EXISTS relay_paranet SMALLINT NOT NULL DEFAULT 0;

-- =============================================================================
-- Index untuk urutkan riwayat terbaru di dashboard
-- =============================================================================
CREATE INDEX IF NOT EXISTS idx_pertanian_created_at_desc ON public.pertanian (created_at DESC);
CREATE INDEX IF NOT EXISTS idx_pertanian_id_desc ON public.pertanian (id DESC);

-- =============================================================================
-- Row Level Security (sesuaikan kebijakan kampus / produksi)
-- Contoh: anon boleh baca + insert — hati-hati untuk aplikasi publik
-- =============================================================================
ALTER TABLE public.pertanian ENABLE ROW LEVEL SECURITY;

DROP POLICY IF EXISTS "pertanian_anon_select" ON public.pertanian;
DROP POLICY IF EXISTS "pertanian_anon_insert" ON public.pertanian;

CREATE POLICY "pertanian_anon_select"
  ON public.pertanian FOR SELECT
  TO anon
  USING (true);

CREATE POLICY "pertanian_anon_insert"
  ON public.pertanian FOR INSERT
  TO anon
  WITH CHECK (true);
