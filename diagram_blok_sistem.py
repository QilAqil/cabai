"""
Diagram blok sistem IoT pertanian cabai rawit (ESP32).
Gaya mengikuti contoh TA: blok bernomor, aliran daya & data.
Keluaran: diagram_blok_sistem.png
"""

import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch

# --- gaya blok (mirip diagram referensi) ---
BOX_KW = dict(
    boxstyle="round,pad=0.35,rounding_size=0.08",
    linewidth=1.4,
    edgecolor="black",
    facecolor="white",
)
PWR_KW = dict(BOX_KW)
PWR_KW["facecolor"] = "#f5f5f5"
CLOUD_KW = dict(BOX_KW)
CLOUD_KW["facecolor"] = "#e8f4fc"


def block(ax, xy, w, h, title, num=None, subtitle=None, style=BOX_KW):
    x, y = xy
    patch = FancyBboxPatch((x, y), w, h, **style)
    ax.add_patch(patch)
    cx = x + w / 2
    cy = y + h / 2
    if num is not None:
        ax.text(
            x + 0.08,
            y + h - 0.12,
            str(num),
            fontsize=11,
            fontweight="bold",
            va="top",
            ha="left",
        )
    lines = [title] if subtitle is None else [title, subtitle]
    fs = 8.5 if subtitle else 9.5
    ax.text(
        cx,
        cy,
        "\n".join(lines),
        ha="center",
        va="center",
        fontsize=fs,
        wrap=True,
    )
    return (x + w / 2, y, x + w / 2, y + h, x, y + h / 2, x + w, y + h / 2)


def arrow(ax, p1, p2, label=None, style="-|>", color="black", lw=1.2):
    arr = FancyArrowPatch(
        p1, p2, arrowstyle=style, mutation_scale=12, linewidth=lw, color=color
    )
    ax.add_patch(arr)
    if label:
        mx = (p1[0] + p2[0]) / 2
        my = (p1[1] + p2[1]) / 2
        ax.text(mx, my + 0.15, label, ha="center", va="bottom", fontsize=7, color=color)


def main():
    fig, ax = plt.subplots(figsize=(14, 9))
    ax.set_xlim(0, 14)
    ax.set_ylim(0, 9)
    ax.axis("off")
    ax.set_title(
        "Diagram Blok Sistem Monitoring & Kontrol Greenhouse Cabai Rawit\n"
        "(ESP32 + Fuzzy Sugeno + MQTT + Supabase)",
        fontsize=13,
        fontweight="bold",
        pad=14,
    )

    # --- pusat: ESP32 + fuzzy ---
    esp = block(
        ax,
        (5.2, 3.6),
        3.6,
        2.4,
        "ESP32",
        1,
        "Fuzzy Sugeno\n(3 jalur)\nGPIO4,13,34,35\n25,26,27",
    )

    # --- sensor (kiri) ---
    b_ph = block(ax, (0.4, 5.2), 2.2, 1.15, "Sensor pH Tanah", 2, "+ driver DMS")
    b_dht = block(ax, (0.4, 3.5), 2.2, 1.15, "DHT22 AM2302", 3, "Suhu & RH udara")
    b_soil = block(ax, (0.4, 1.8), 2.2, 1.15, "Soil Moisture", 4, "Probe + modul AO")
    b_llc = block(
        ax,
        (2.9, 2.8),
        1.5,
        2.0,
        "Level Shifter\n(opsional)",
        5,
        "5V → 3,3V\nanalog pH",
    )

    # --- daya (atas) ---
    b_pwr = block(ax, (5.5, 7.0), 2.8, 0.9, "Adaptor 12V DC", 8, style=PWR_KW)
    b_buck = block(ax, (5.5, 5.85), 2.8, 0.9, "DC-DC Step Down", 9, "5V → sensor", style=PWR_KW)

    # --- aktuator (kanan bawah) ---
    b_servo = block(ax, (10.2, 4.5), 2.4, 1.0, "Servo Paranet", 10, "GPIO25")
    b_r_air = block(ax, (10.2, 3.2), 2.4, 1.0, "Relay Air", 11, "GPIO26 · pompa")
    b_r_ph = block(ax, (10.2, 1.9), 2.4, 1.0, "Relay pH", 12, "GPIO27 · koreksi")

    # --- cloud & dashboard (kanan atas) ---
    b_db = block(ax, (10.0, 6.5), 2.6, 1.0, "Supabase", 7, "REST log", style=CLOUD_KW)
    b_mqtt = block(ax, (10.0, 5.2), 2.6, 1.0, "Broker MQTT", 13, "EMQX TLS", style=CLOUD_KW)
    b_dash = block(
        ax,
        (10.0, 7.8),
        2.6,
        1.15,
        "Dashboard Web",
        14,
        "index.html\nreal-time + riwayat",
        style=CLOUD_KW,
    )

    # --- panah data sensor → ESP32 ---
    arrow(ax, (2.6, 5.75), (5.2, 4.8), color="#333")
    arrow(ax, (2.6, 4.05), (5.2, 4.5))
    arrow(ax, (2.6, 2.35), (5.2, 4.2))
    arrow(ax, (4.4, 3.8), (5.2, 4.3), label="AO", color="#555")

    # --- daya ---
    arrow(ax, (6.9, 7.0), (6.9, 6.75), style="-", color="#666")
    arrow(ax, (6.9, 5.85), (2.6, 6.2), label="5V", color="#666")
    arrow(ax, (6.9, 5.85), (6.9, 6.0), color="#666")
    arrow(ax, (7.8, 7.45), (7.8, 6.75), label="12V", color="#666")
    arrow(ax, (8.8, 7.0), (8.8, 4.8), label="USB/5V", color="#666", lw=1.0)

    # --- ESP32 → aktuator ---
    arrow(ax, (8.8, 4.5), (10.2, 5.0))
    arrow(ax, (8.8, 4.2), (10.2, 3.7))
    arrow(ax, (8.8, 3.9), (10.2, 2.4))

    # --- ESP32 → cloud (WiFi) ---
    arrow(ax, (8.8, 5.0), (10.0, 5.7), label="WiFi", color="#1565c0")
    arrow(ax, (8.8, 4.8), (10.0, 6.9), label="HTTPS", color="#1565c0")
    arrow(ax, (11.3, 6.5), (11.3, 7.8), label="SELECT", color="#1565c0")
    arrow(ax, (11.3, 5.7), (11.3, 7.8), color="#1565c0", lw=0.9)

    # legenda singkat
    ax.text(
        0.35,
        0.35,
        "Alur: Sensor → ESP32 (fuzzy) → Servo/Relay | MQTT & Supabase → Dashboard\n"
        "Greenhouse cabai rawit · DHT22 · kelembaban tanah · pH",
        fontsize=8,
        va="bottom",
        style="italic",
        color="#444",
    )

    out = "diagram_blok_sistem.png"
    plt.tight_layout()
    plt.savefig(out, dpi=180, bbox_inches="tight", facecolor="white")
    print(f"Disimpan: {out}")
    plt.close()


if __name__ == "__main__":
    main()
