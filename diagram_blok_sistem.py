"""
Diagram blok: monitoring & kontrol greenhouse cabai rawit
Selaras index.ino + index.html (ESP32, 3 sensor, Fuzzy Tahani Mamdani, servo, 2 relay, MQTT, Supabase).

Jalankan: python diagram_blok_sistem.py
Keluaran: diagram_blok_sistem.png
"""

import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Circle, Rectangle

BOX = dict(boxstyle="round,pad=0.4,rounding_size=0.1", linewidth=1.5, edgecolor="#1a1a1a", facecolor="white")
ESP = dict(boxstyle="round,pad=0.5,rounding_size=0.12", linewidth=2.0, edgecolor="#1565c0", facecolor="#e3f2fd")
DB = dict(boxstyle="round,pad=0.4,rounding_size=0.1", linewidth=1.5, edgecolor="#2e7d32", facecolor="#e8f5e9")
NET = dict(boxstyle="round,pad=0.4,rounding_size=0.1", linewidth=1.5, edgecolor="#6a1b9a", facecolor="#f3e5f5")
PWR = dict(boxstyle="round,pad=0.35,rounding_size=0.08", linewidth=1.3, edgecolor="#555", facecolor="#f5f5f5")


def block(ax, cx, cy, w, h, text, style=BOX, fs=9):
    x, y = cx - w / 2, cy - h / 2
    ax.add_patch(FancyBboxPatch((x, y), w, h, **style))
    ax.text(cx, cy, text, ha="center", va="center", fontsize=fs, wrap=True)


def arrow(ax, x1, y1, x2, y2, label=None, color="#333", lw=1.4):
    ax.add_patch(
        FancyArrowPatch(
            (x1, y1), (x2, y2),
            arrowstyle="-|>", mutation_scale=14, linewidth=lw, color=color,
            connectionstyle="arc3,rad=0.0",
        )
    )
    if label:
        mx, my = (x1 + x2) / 2, (y1 + y2) / 2
        ax.text(mx, my + 0.12, label, ha="center", va="bottom", fontsize=7, color=color)


def main():
    fig, ax = plt.subplots(figsize=(13, 9))
    ax.set_xlim(0, 13)
    ax.set_ylim(0, 9)
    ax.axis("off")
    ax.set_title(
        "Diagram Blok Sistem Monitoring & Penanganan Tanaman Cabai Rawit\n"
        "(Sesuai firmware index.ino & dashboard index.html)",
        fontsize=12, fontweight="bold", pad=12,
    )

    # --- Pusat: ESP32 + Fuzzy ---
    esp_x, esp_y = 6.5, 4.5
    block(
        ax, esp_x, esp_y, 3.2, 2.2,
        "ESP32\n+ Fuzzy Tahani\n(3 jalur)\nGPIO4,13,34,35\n21,26,27",
        style=ESP, fs=8.5,
    )

    # --- Sensor (atas) → masuk ke ESP32 ---
    sy = 7.2
    block(ax, 3.0, sy, 2.4, 1.0, "Sensor\nDHT22\n(suhu & RH udara)", fs=8)
    block(ax, 6.5, sy, 2.5, 1.0, "Sensor\nSoil Moisture\n(kelembaban tanah %)", fs=8)
    block(ax, 10.0, sy, 2.4, 1.0, "Sensor pH Tanah\n+ driver DMS", fs=8)

    arrow(ax, 3.0, 6.65, esp_x - 1.0, esp_y + 1.0, "data", "#333")
    arrow(ax, 6.5, 6.65, esp_x, esp_y + 1.1, "AO", "#333")
    arrow(ax, 10.0, 6.65, esp_x + 1.0, esp_y + 1.0, "ADC", "#333")

    # --- Aktuator (kanan) ← ESP32 ---
    ax_r = 10.8
    block(ax, ax_r, 6.0, 2.2, 0.85, "Servo Paranet\n3,3 V · GPIO21", fs=7.5)
    block(ax, ax_r, 4.7, 2.2, 0.85, "Relay Air\n5 V · GPIO26", fs=7.5)
    block(ax, ax_r, 3.4, 2.2, 0.85, "Relay pH\n5 V · GPIO27", fs=7.5)
    block(ax, 11.9, 4.7, 1.5, 0.75, "Pompa\npenyiraman", fs=7.5)
    block(ax, 11.9, 3.4, 1.5, 0.75, "Pompa / larutan\nkoreksi pH", fs=7.5)

    arrow(ax, esp_x + 1.6, esp_y + 0.6, ax_r - 1.1, 6.0, "PWM", "#c62828")
    arrow(ax, esp_x + 1.6, esp_y + 0.1, ax_r - 1.1, 4.7, color="#c62828")
    arrow(ax, esp_x + 1.6, esp_y - 0.4, ax_r - 1.1, 3.4, color="#c62828")
    arrow(ax, ax_r + 1.1, 4.7, 11.15, 4.7, color="#c62828", lw=1.1)
    arrow(ax, ax_r + 1.1, 3.4, 11.15, 3.4, color="#c62828", lw=1.1)

    # Label jalur fuzzy (kiri bawah ESP)
    ax.text(
        4.2, 3.35,
        "Fuzzy:\n• suhu → paranet\n• tanah → air\n• pH → koreksi",
        fontsize=7.5, ha="left", va="top",
        bbox=dict(boxstyle="round", facecolor="#fff8e1", edgecolor="#ffb300", alpha=0.95),
    )

    # --- Database & cloud (kiri + atas kanan) ---
    block(ax, 1.8, 5.0, 2.4, 1.0, "Database\nSupabase\ntabel pertanian", style=DB, fs=8)
    block(ax, 1.8, 3.2, 2.4, 1.0, "Broker MQTT\n(EMQX TLS)\ntopic pertanian/sensor", style=NET, fs=7.5)
    block(ax, 1.8, 1.5, 2.4, 1.1, "Dashboard Web\nindex.html\nMQTT + REST", style=NET, fs=8)

    arrow(ax, esp_x - 1.6, esp_y + 0.3, 3.0, 5.0, "HTTPS\nPOST", "#1565c0")
    arrow(ax, esp_x - 1.6, esp_y + 0.8, 3.0, 3.2, "WiFi\npublish", "#1565c0")
    arrow(ax, 3.0, 4.45, 3.0, 3.75, "subscribe", "#6a1b9a", lw=1.0)
    arrow(ax, 3.0, 2.65, 3.0, 2.05, "SELECT\nriwayat", "#6a1b9a", lw=1.0)

    # --- Daya (bawah): 12V → expansion board → 3,3V (sensor+ESP32+servo) & 5V (relay saja) ---
    block(ax, 4.5, 1.05, 2.6, 0.8, "Adaptor\n12 V DC", style=PWR, fs=8.5)
    block(ax, 7.5, 1.05, 3.0, 0.8, "ESP32 30P Board\n(regulator onboard)", style=PWR, fs=8)
    block(ax, 10.8, 1.55, 2.0, 0.65, "Rail 3,3 V\nESP32 · sensor · servo", style=PWR, fs=7.5)
    block(ax, 10.8, 0.55, 2.0, 0.65, "Rail 5 V\nrelay air & pH", style=PWR, fs=7.5)

    arrow(ax, 5.8, 1.05, 6.0, 1.05, color="#666")
    arrow(ax, 9.0, 1.25, esp_x, esp_y - 1.1, "3,3 V", "#2e7d32")
    arrow(ax, 9.0, 1.05, 10.8, 1.55, color="#2e7d32", lw=1.1)
    arrow(ax, 9.0, 0.85, 10.8, 0.55, "5 V", "#c62828", lw=1.1)
    arrow(ax, 11.5, 1.85, 6.5, 6.65, color="#2e7d32", lw=0.9)
    arrow(ax, 11.5, 0.85, ax_r, 4.1, color="#c62828", lw=0.9)

    ax.text(
        0.25, 0.2,
        "Daya: tanpa step-down eksternal. Sensor & servo 3,3 V; hanya modul relay 5 V. Cabai rawit.",
        fontsize=7, va="bottom", color="#444", style="italic",
    )

    out = "diagram_blok_sistem.png"
    plt.tight_layout()
    plt.savefig(out, dpi=200, bbox_inches="tight", facecolor="white")
    print(f"Disimpan: {out}")


if __name__ == "__main__":
    import matplotlib
    matplotlib.use("Agg")
    main()
