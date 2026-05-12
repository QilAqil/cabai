import numpy as np
import matplotlib.pyplot as plt

# =========================
# FUNGSI KEANGGOTAAN FUZZY
# =========================
def left_shoulder(x, a, b):
    """Bahu kiri: 1 turun ke 0"""
    return np.piecewise(
        x,
        [x <= a, (x > a) & (x < b), x >= b],
        [1, lambda x: (b - x) / (b - a), 0]
    )

def triangle(x, a, b, c):
    """Segitiga"""
    return np.piecewise(
        x,
        [x <= a, (x > a) & (x <= b), (x > b) & (x < c), x >= c],
        [0, lambda x: (x - a) / (b - a), lambda x: (c - x) / (c - b), 0]
    )

def trapezoid(x, a, b, c, d):
    """Trapesium"""
    return np.piecewise(
        x,
        [x <= a, (x > a) & (x < b), (x >= b) & (x <= c), (x > c) & (x < d), x >= d],
        [0, lambda x: (x - a) / (b - a), 1, lambda x: (d - x) / (d - c), 0]
    )

def right_shoulder(x, a, b):
    """Bahu kanan: 0 naik ke 1"""
    return np.piecewise(
        x,
        [x <= a, (x > a) & (x < b), x >= b],
        [0, lambda x: (x - a) / (b - a), 1]
    )

# =========================
# 1. SUHU UDARA
# =========================
x_suhu = np.linspace(0, 40, 500)

suhu_rendah = left_shoulder(x_suhu, 24, 27)
suhu_sedang = triangle(x_suhu, 24, 27, 31)
suhu_tinggi = right_shoulder(x_suhu, 27, 31)

# =========================
# 2. KELEMBABAN TANAH
# =========================
x_kelembaban = np.linspace(0, 100, 500)

kering = left_shoulder(x_kelembaban, 40, 50)
lembab = trapezoid(x_kelembaban, 40, 50, 70, 80)
basah = right_shoulder(x_kelembaban, 70, 80)

# =========================
# 3. pH TANAH
# =========================
x_ph = np.linspace(3, 9, 500)

asam = left_shoulder(x_ph, 5, 6)
netral = trapezoid(x_ph, 5.5, 6, 7, 7.5)
basa = right_shoulder(x_ph, 7, 7.5)

# =========================
# PLOT GRAFIK
# =========================
plt.figure(figsize=(12, 10))

# --- Suhu Udara ---
plt.subplot(3, 1, 1)
plt.plot(x_suhu, suhu_rendah, 'k', linewidth=2, label='Rendah')
plt.plot(x_suhu, suhu_sedang, 'k', linewidth=2, label='Sedang')
plt.plot(x_suhu, suhu_tinggi, 'k', linewidth=2, label='Tinggi')
plt.title('Fungsi Keanggotaan Fuzzy - Suhu Udara')
plt.ylabel('Derajat Keanggotaan μ(x)')
plt.xlabel('Suhu (°C)')
plt.xlim(0, 40)
plt.ylim(0, 1.1)
plt.grid(True)
plt.legend()

# --- Kelembaban Tanah ---
plt.subplot(3, 1, 2)
plt.plot(x_kelembaban, kering, 'k', linewidth=2, label='Kering')
plt.plot(x_kelembaban, lembab, 'k', linewidth=2, label='Lembab')
plt.plot(x_kelembaban, basah, 'k', linewidth=2, label='Basah')
plt.title('Fungsi Keanggotaan Fuzzy - Kelembaban Tanah')
plt.ylabel('Derajat Keanggotaan μ(x)')
plt.xlabel('Kelembaban (%)')
plt.xlim(0, 100)
plt.ylim(0, 1.1)
plt.grid(True)
plt.legend()

# --- pH Tanah ---
plt.subplot(3, 1, 3)
plt.plot(x_ph, asam, 'k', linewidth=2, label='Asam')
plt.plot(x_ph, netral, 'k', linewidth=2, label='Netral')
plt.plot(x_ph, basa, 'k', linewidth=2, label='Basa')
plt.title('Fungsi Keanggotaan Fuzzy - pH Tanah')
plt.ylabel('Derajat Keanggotaan μ(x)')
plt.xlabel('pH')
plt.xlim(3, 9)
plt.ylim(0, 1.1)
plt.grid(True)
plt.legend()

plt.tight_layout()
plt.show()