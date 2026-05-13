-- =============================================================================
-- Sinkronkan tabel public.pertanian dengan index.ino / index.html (ESP32 terbaru).
-- Cocok untuk skema seperti di dashboard Supabase Anda:
--   id, updated_at, temperature, humidity, soil, ph,
--   fuzzy_air, fuzzy_ph, relay_air, relay_dolomit, relay_paranet
--
-- Yang dilakukan (aman dijalankan berulang):
--   1) fuzzy_air → fuzzy_suhu (nama kolom selaras JSON ESP32)
--   2) Tambah fuzzy_soil jika belum ada (skor siram dari kelembaban tanah)
--   3) Tambah relay_paranet jika belum ada (Anda sudah punya — baris ini no-op)
-- =============================================================================

-- 1) Rename fuzzy_air → fuzzy_suhu hanya jika kolom lama masih ada
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

COMMENT ON COLUMN public.pertanian.fuzzy_suhu IS 'Skor fuzzy suhu udara → paranet (0–1)';

-- 2) Kolom baru untuk firmware tiga-jalur (tanah → air), tipe REAL = float4 seperti kolom Anda
ALTER TABLE public.pertanian
  ADD COLUMN IF NOT EXISTS fuzzy_soil REAL NOT NULL DEFAULT 0;

COMMENT ON COLUMN public.pertanian.fuzzy_soil IS 'Skor fuzzy kelembaban tanah → kebutuhan air (0–1)';

-- 3) Paranet — jika sudah ada, pernyataan ini tidak mengubah apa pun
ALTER TABLE public.pertanian
  ADD COLUMN IF NOT EXISTS relay_paranet REAL NOT NULL DEFAULT 0;

COMMENT ON COLUMN public.pertanian.relay_paranet IS '1 = relay paranet ON';
