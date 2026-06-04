"""
Diagram desain perangkat IoT (gaya sederhana seperti contoh gambar).

Output:
  - desain_perangkat_iot_contoh.png
"""

from __future__ import annotations


def main() -> None:
    import matplotlib.pyplot as plt
    from matplotlib.patches import FancyArrowPatch, FancyBboxPatch, Rectangle

    def box(ax, x, y, w, h, text, fontsize=10):
        p = FancyBboxPatch(
            (x, y),
            w,
            h,
            boxstyle="round,pad=0.012,rounding_size=0.012",
            linewidth=1.2,
            edgecolor="black",
            facecolor="white",
        )
        ax.add_patch(p)
        ax.text(x + w / 2, y + h / 2, text, ha="center", va="center", fontsize=fontsize, color="black")
        return (x, y, w, h)

    def big_box(ax, x, y, w, h, text, fontsize=11):
        p = Rectangle((x, y), w, h, linewidth=1.2, edgecolor="black", facecolor="white")
        ax.add_patch(p)
        ax.text(x + w / 2, y + h / 2, text, ha="center", va="center", fontsize=fontsize, color="black")
        return (x, y, w, h)

    def mid_top(r):
        x, y, w, h = r
        return (x + w / 2, y + h)

    def mid_bottom(r):
        x, y, w, h = r
        return (x + w / 2, y)

    def mid_left(r):
        x, y, w, h = r
        return (x, y + h / 2)

    def mid_right(r):
        x, y, w, h = r
        return (x + w, y + h / 2)

    def arrow(ax, p1, p2):
        ax.add_patch(
            FancyArrowPatch(
                p1,
                p2,
                arrowstyle="-|>",
                mutation_scale=12,
                linewidth=1.1,
                color="black",
                connectionstyle="arc3,rad=0.0",
            )
        )

    fig = plt.figure(figsize=(12, 6.6), dpi=180)
    ax = fig.add_subplot(111)
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")

    # ===== Layout mirip contoh =====
    dht = box(ax, 0.15, 0.86, 0.16, 0.07, "Sensor DHT22")
    soil = box(ax, 0.38, 0.86, 0.18, 0.07, "Sensor Soil Moisture")
    ph = box(ax, 0.63, 0.86, 0.16, 0.07, "Sensor pH Tanah + DMS")

    esp32 = big_box(ax, 0.38, 0.40, 0.24, 0.32, "ESP32")

    db = box(ax, 0.08, 0.47, 0.14, 0.07, "Database\n(Supabase)")
    power = box(ax, 0.40, 0.20, 0.20, 0.07, "Sumber Daya/Adaptor\n(12V → board)")

    relay = box(ax, 0.70, 0.52, 0.12, 0.07, "Relay")
    pump1 = box(ax, 0.86, 0.60, 0.12, 0.07, "Pompa Mini\n(Air)")
    pump2 = box(ax, 0.86, 0.48, 0.12, 0.07, "Pompa Mini\n(Koreksi pH)")
    blower = box(ax, 0.86, 0.34, 0.12, 0.07, "Blower")

    # ===== Koneksi (panah) =====
    # Sensor -> ESP32
    arrow(ax, mid_bottom(dht), (mid_top(esp32)[0] - 0.09, mid_top(esp32)[1]))
    arrow(ax, mid_bottom(soil), (mid_top(esp32)[0], mid_top(esp32)[1]))
    arrow(ax, mid_bottom(ph), (mid_top(esp32)[0] + 0.09, mid_top(esp32)[1]))

    # Power -> ESP32
    arrow(ax, mid_top(power), mid_bottom(esp32))

    # ESP32 -> Database
    arrow(ax, mid_left(esp32), mid_right(db))

    # ESP32 -> Relay
    arrow(ax, mid_right(esp32), mid_left(relay))

    # Relay -> Pumps
    arrow(ax, mid_right(relay), mid_left(pump1))
    arrow(ax, mid_right(relay), mid_left(pump2))

    # ESP32 -> Blower (relay blower)
    arrow(ax, (mid_right(esp32)[0], mid_right(esp32)[1] - 0.12), mid_left(blower))

    ax.text(
        0.5,
        0.98,
        "Desain Perangkat IoT Greenhouse Cabai Rawit",
        ha="center",
        va="top",
        fontsize=14,
        fontweight="bold",
        color="black",
    )

    fig.savefig("desain_perangkat_iot_contoh.png", bbox_inches="tight")
    print("OK: wrote desain_perangkat_iot_contoh.png")


if __name__ == "__main__":
    main()

