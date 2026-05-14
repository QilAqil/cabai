-- =============================================================================
-- Sinkronkan tabel public.pertanian dengan index.ino / index.html.
-- Skema umum di Supabase: id, updated_at, temperature, humidity, soil, ph,
--   fuzzy_suhu, fuzzy_ph, relay_air, relay_dolomit, relay_paranet, fuzzy_soil (float4).
--
-- Yang dilakukan (aman dijalankan berulang):
--   1) fuzzy_air → fuzzy_suhu (jika kolom lama masih ada)
--   2) Tambah fuzzy_soil jika belum ada
--   3) Tambah relay_paranet jika belum ada (flag servo paranet 0/1)
-- =============================================================================

-- 1) Rename fuzzy_air → fuzzy_suhu
DO $$
BEGIN
  IF EXISTS (
    SELECT 1
    FROM information_schema.columns
    WHERE table_schema = 'public'
      AND table_name = 'pertanian'
      AND column_name = 'fuzzy_air'
  ) THEN
    ALTER TABLE public.pertanian RENAME COLUMN fuzzy_air TO fuzzy_suhu;
  END IF;
END $$;

COMMENT ON COLUMN public.pertanian.fuzzy_suhu IS 'Skor fuzzy suhu → servo paranet (0–1)';

-- 2) Skor fuzzy tanah → air
ALTER TABLE public.pertanian
  ADD COLUMN IF NOT EXISTS fuzzy_soil REAL NOT NULL DEFAULT 0;

COMMENT ON COLUMN public.pertanian.fuzzy_soil IS 'Skor fuzzy kelembaban tanah → relay air (0–1)';

-- 3) Flag servo paranet (nama kolom relay_paranet — selaras DB Anda)
ALTER TABLE public.pertanian
  ADD COLUMN IF NOT EXISTS relay_paranet REAL NOT NULL DEFAULT 0;

COMMENT ON COLUMN public.pertanian.relay_paranet IS '0/1 flag servo paranet ON';
