-- =============================================================================
-- Skema tabel `pertanian` — selaras dashboard Supabase (float4 pada sensor/relay).
-- Migrasi dari tabel lama: supabase/migration_sync_pertanian_for_esp32.sql
-- ESP32 POST: fuzzy_suhu, fuzzy_soil, fuzzy_ph, relay_paranet (0/1 = servo paranet), relay_air, relay_dolomit
-- =============================================================================

-- Tabel baru (jika belum ada)
CREATE TABLE IF NOT EXISTS public.pertanian (
  id              BIGSERIAL PRIMARY KEY,
  created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),

  -- Sensor (di UI Supabase sering tampil sebagai float4)
  temperature     REAL NOT NULL DEFAULT 0,
  humidity        REAL NOT NULL DEFAULT 0,
  soil            REAL NOT NULL DEFAULT 0,
  ph              REAL NOT NULL DEFAULT 0,

  -- Skor fuzzy 0–1
  fuzzy_suhu      REAL NOT NULL DEFAULT 0,
  fuzzy_soil      REAL NOT NULL DEFAULT 0,
  fuzzy_ph        REAL NOT NULL DEFAULT 0,

  -- relay_paranet: nama kolom legacy; isi 0/1 = flag servo paranet (bukan relay fisik)
  relay_paranet   REAL NOT NULL DEFAULT 0,
  relay_air       REAL NOT NULL DEFAULT 0,
  relay_dolomit   REAL NOT NULL DEFAULT 0
);

COMMENT ON TABLE public.pertanian IS 'Log sensor & kontrol IoT pertanian (MQTT + Supabase)';
COMMENT ON COLUMN public.pertanian.fuzzy_suhu IS 'Skor fuzzy suhu → servo paranet (0–1)';
COMMENT ON COLUMN public.pertanian.fuzzy_soil IS 'Skor fuzzy tanah → relay air (0–1)';
COMMENT ON COLUMN public.pertanian.fuzzy_ph IS 'Skor fuzzy pH → relay koreksi pH (0–1)';
COMMENT ON COLUMN public.pertanian.relay_paranet IS '0/1 flag servo paranet ON (nama kolom tetap di DB)';
COMMENT ON COLUMN public.pertanian.relay_dolomit IS '0/1 relay koreksi pH (legacy: dolomit)';

-- =============================================================================
-- Migrasi dari tabel lama: tambah kolom yang belum ada (aman dijalankan ulang)
-- =============================================================================
ALTER TABLE public.pertanian ADD COLUMN IF NOT EXISTS fuzzy_soil REAL NOT NULL DEFAULT 0;
ALTER TABLE public.pertanian ADD COLUMN IF NOT EXISTS relay_paranet REAL NOT NULL DEFAULT 0;

-- =============================================================================
-- Index untuk urutkan riwayat terbaru di dashboard
-- =============================================================================
CREATE INDEX IF NOT EXISTS idx_pertanian_created_at_desc ON public.pertanian (created_at DESC);
CREATE INDEX IF NOT EXISTS idx_pertanian_id_desc ON public.pertanian (id DESC);

-- =============================================================================
-- Row Level Security (sesuaikan kebijakan kampus / produksi)
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
