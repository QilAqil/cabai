"""
Flowchart alur sistem — gaya sederhana seperti contoh TA.
Start → 3 sensor paralel → kirim database → tampilan + fuzzy.

Jalankan: python flowchart_sistem.py
Keluaran: flowchart_sistem.png
"""

from __future__ import annotations

import matplotlib.pyplot as plt
from matplotlib.patches import Ellipse, FancyArrowPatch, FancyBboxPatch, Polygon, Rectangle


def terminal(ax, cx, cy, w, h, text, fs=10):
    ax.add_patch(Ellipse((cx, cy), w, h, facecolor="white", edgecolor="black", linewidth=1.5))
    ax.text(cx, cy, text, ha="center", va="center", fontsize=fs, fontweight="bold")


def process(ax, cx, cy, w, h, text, fs=8.5):
    x, y = cx - w / 2, cy - h / 2
    ax.add_patch(
        FancyBboxPatch(
            (x, y), w, h,
            boxstyle="square,pad=0.3",
            facecolor="white",
            edgecolor="black",
            linewidth=1.4,
        )
    )
    ax.text(cx, cy, text, ha="center", va="center", fontsize=fs)


def data_io(ax, cx, cy, w, h, text, fs=8.5):
    dx = w * 0.12
    pts = [
        (cx - w / 2 + dx, cy - h / 2),
        (cx + w / 2 + dx, cy - h / 2),
        (cx + w / 2 - dx, cy + h / 2),
        (cx - w / 2 - dx, cy + h / 2),
    ]
    ax.add_patch(Polygon(pts, closed=True, facecolor="#f5f5f5", edgecolor="black", linewidth=1.4))
    ax.text(cx, cy, text, ha="center", va="center", fontsize=fs)


def database(ax, cx, cy, w, h, text, fs=9):
    body_h = h - h * 0.22
    eh = h * 0.22
    ax.add_patch(Ellipse((cx, cy + body_h / 2), w, eh, facecolor="#e8f5e9", edgecolor="black", linewidth=1.4))
    ax.add_patch(Rectangle((cx - w / 2, cy - body_h / 2), w, body_h, facecolor="#e8f5e9", edgecolor="black", linewidth=1.4))
    ax.add_patch(Ellipse((cx, cy - body_h / 2), w, eh, facecolor="#c8e6c9", edgecolor="black", linewidth=1.4))
    ax.text(cx, cy, text, ha="center", va="center", fontsize=fs, fontweight="bold")


def arrow(ax, x1, y1, x2, y2):
    ax.add_patch(
        FancyArrowPatch(
            (x1, y1), (x2, y2),
            arrowstyle="-|>",
            mutation_scale=12,
            linewidth=1.3,
            color="#333",
        )
    )


def main() -> None:
    fig, ax = plt.subplots(figsize=(9, 11))
    ax.set_xlim(0, 9)
    ax.set_ylim(0, 12)
    ax.axis("off")
    ax.set_title(
        "Flowchart Sistem Monitoring Greenhouse Cabai Rawit",
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    cx = 4.5

    # 1. Start
    terminal(ax, cx, 11.2, 1.8, 0.6, "Start")

    # 2. Tiga cabang sensor (paralel)
    xs = [1.8, 4.5, 7.2]
    reads = [
        "Pembacaan\nSensor pH",
        "Pembacaan\nSensor Suhu",
        "Pembacaan\nSensor\nKelembaban Tanah",
    ]
    values = ["Nilai pH", "Nilai Suhu", "Nilai\nKelembaban\nTanah"]

    for x, rd, val in zip(xs, reads, values):
        arrow(ax, cx, 10.9, x, 10.15)
        process(ax, x, 9.55, 2.1, 0.85, rd)
        arrow(ax, x, 9.1, x, 8.65)
        data_io(ax, x, 8.2, 1.95, 0.72, val)

    # 3. Gabung → kirim database
    for x in xs:
        arrow(ax, x, 7.85, cx, 7.35)
    process(ax, cx, 6.75, 4.2, 0.85, "Proses Pengiriman\nke dalam Database")
    arrow(ax, cx, 6.3, cx, 5.85)
    database(ax, cx, 5.15, 2.4, 1.0, "Database")

    # 4. Dua cabang dari database (seperti contoh: LCD + Fuzzy)
    db_y = 4.65
    arrow(ax, cx, db_y, 2.2, 4.15)
    arrow(ax, cx, db_y, 6.8, 4.15)

    process(
        ax, 2.2, 3.55, 3.0, 0.95,
        "Menampilkan Data Sensor\ndi Dashboard Web",
    )

    process(ax, 6.8, 3.55, 3.0, 0.85, "Pengolahan Data\nFuzzy Tahani")
    arrow(ax, 6.8, 3.1, 6.8, 2.7)
    data_io(ax, 6.8, 2.25, 2.4, 0.65, "Kualitas\nLingkungan")
    arrow(ax, 6.8, 1.9, 6.8, 1.5)
    process(ax, 6.8, 0.95, 3.0, 0.85, "Kendali Aktuator\n(Relay Blower · Air · pH)")

    out = "flowchart_sistem.png"
    plt.tight_layout()
    plt.savefig(out, dpi=200, bbox_inches="tight", facecolor="white")
    print(f"Disimpan: {out}")


if __name__ == "__main__":
    import matplotlib
    matplotlib.use("Agg")
    main()
