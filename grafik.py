"""
Grafik fungsi keanggotaan Fuzzy Tahani (Mamdani) — selaras index.ino
- Baris 1–3: fuzzifikasi INPUT (suhu, tanah, pH)
- Baris 4–6: konsekuen OUTPUT domain [0,1] (intensitas aktuator)

Jalankan: python grafik.py
Keluaran: grafik_fuzzy_input.png, grafik_fuzzy_output.png
"""

import numpy as np
import matplotlib.pyplot as plt

# =========================
# trapmf / trimf (sama logika index.ino)
# =========================
def trimf(x, a, b, c):
    y = np.zeros_like(x, dtype=float)
    rising = (x > a) & (x < b)
    if b > a:
        y[rising] = (x[rising] - a) / (b - a)
    falling = (x > b) & (x < c)
    if c > b:
        y[falling] = (c - x[falling]) / (c - b)
    y[x == b] = 1.0
    return np.clip(y, 0, 1)


def trapmf(x, a, b, c, d):
    y = np.zeros_like(x, dtype=float)
    outside = (x <= a) | (x >= d)
    y[outside] = 0.0
    plateau = (x >= b) & (x <= c)
    y[plateau] = 1.0
    left = (x > a) & (x < b)
    if b > a:
        y[left] = (x[left] - a) / (b - a)
    right = (x > c) & (x < d)
    if d > c:
        y[right] = (d - x[right]) / (d - c)
    return np.clip(y, 0, 1)


# --- INPUT (fuzzifikasi) — parameter = index.ino ---
x_suhu = np.linspace(0, 45, 500)
suhu_rendah = trapmf(x_suhu, 0, 0, 24, 27)
suhu_sedang = trimf(x_suhu, 24, 27, 31)
suhu_tinggi = trapmf(x_suhu, 27, 31, 45, 45)

x_tanah = np.linspace(0, 100, 500)
kering = trapmf(x_tanah, 0, 0, 40, 50)
lembab = trapmf(x_tanah, 40, 50, 70, 80)
basah = trapmf(x_tanah, 70, 80, 100, 100)

x_ph = np.linspace(0, 9, 500)
asam = trapmf(x_ph, 3, 3, 5, 6)  # sama index.ino (phValid 3–9)
netral = trapmf(x_ph, 5.5, 6, 7, 7.5)
basa = trapmf(x_ph, 7, 7.5, 9, 9)

# --- OUTPUT (konsekuen Tahani) — outMuByKind di index.ino ---
x_out = np.linspace(0, 1, 500)
out_rendah = trapmf(x_out, 0, 0, 0.15, 0.45)
out_sedang = trimf(x_out, 0.15, 0.35, 0.55)
out_tinggi = trapmf(x_out, 0.45, 0.65, 1, 1)

PLOT = dict(color="k", linewidth=2)


def plot_input():
    fig, axes = plt.subplots(3, 1, figsize=(10, 10))
    fig.suptitle(
        "Fuzzy Tahani — Fuzzifikasi INPUT (sesuai index.ino)",
        fontsize=12, fontweight="bold", y=1.01,
    )

    axes[0].plot(x_suhu, suhu_rendah, label="Rendah", **PLOT)
    axes[0].plot(x_suhu, suhu_sedang, label="Sedang", **PLOT)
    axes[0].plot(x_suhu, suhu_tinggi, label="Tinggi", **PLOT)
    axes[0].set_title("Suhu Udara (DHT22)")
    axes[0].set_xlabel("Suhu (°C)")
    axes[0].set_xlim(0, 45)
    axes[0].set_ylim(0, 1.1)
    axes[0].set_ylabel("μ(x)")
    axes[0].grid(True)
    axes[0].legend()

    axes[1].plot(x_tanah, kering, label="Kering", **PLOT)
    axes[1].plot(x_tanah, lembab, label="Lembab", **PLOT)
    axes[1].plot(x_tanah, basah, label="Basah", **PLOT)
    axes[1].set_title("Kelembaban Tanah (%)")
    axes[1].set_xlabel("Kelembaban (%)")
    axes[1].set_xlim(0, 100)
    axes[1].set_ylim(0, 1.1)
    axes[1].set_ylabel("μ(x)")
    axes[1].grid(True)
    axes[1].legend()

    axes[2].plot(x_ph, asam, label="Asam", **PLOT)
    axes[2].plot(x_ph, netral, label="Netral", **PLOT)
    axes[2].plot(x_ph, basa, label="Basa", **PLOT)
    axes[2].set_title("pH Tanah")
    axes[2].set_xlabel("pH")
    axes[2].set_xlim(0, 9)
    axes[2].set_ylim(0, 1.1)
    axes[2].set_ylabel("μ(x)")
    axes[2].grid(True)
    axes[2].legend()

    plt.tight_layout()
    out = "grafik_fuzzy_input.png"
    plt.savefig(out, dpi=180, bbox_inches="tight", facecolor="white")
    print(f"Disimpan: {out}")
    plt.close()


def plot_output():
    fig, ax = plt.subplots(figsize=(8, 4))
    ax.plot(x_out, out_rendah, label="Intensitas Rendah (mis. tidak perlu aktuator)", **PLOT)
    ax.plot(x_out, out_sedang, label="Intensitas Sedang", **PLOT)
    ax.plot(x_out, out_tinggi, label="Intensitas Tinggi (mis. perlu aktuator)", **PLOT)
    ax.set_title(
        "Fuzzy Tahani — Konsekuen OUTPUT (domain skor 0–1)\n"
        "Digunakan di implikasi MIN → agregasi MAX → centroid",
        fontsize=11, fontweight="bold",
    )
    ax.set_xlabel("Skor keluaran y")
    ax.set_ylabel("μ(y)")
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1.1)
    ax.grid(True)
    ax.legend(loc="center right", fontsize=8)
    plt.tight_layout()
    out = "grafik_fuzzy_output.png"
    plt.savefig(out, dpi=180, bbox_inches="tight", facecolor="white")
    print(f"Disimpan: {out}")
    plt.close()


if __name__ == "__main__":
    import matplotlib
    matplotlib.use("Agg")
    plot_input()
    plot_output()
