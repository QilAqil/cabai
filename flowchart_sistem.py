"""
Flowchart alur sistem monitoring & kontrol cabai rawit
Sesuai index.ino + index.html (Fuzzy Tahani Mamdani · tanpa LCD / Telegram).

Jalankan: python flowchart_sistem.py
Keluaran: flowchart_sistem.png
"""

import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Circle, Polygon, Ellipse, Rectangle, FancyArrowPatch

# --- bentuk flowchart ---
def terminal(ax, cx, cy, w, h, text, fs=9):
    e = Ellipse((cx, cy), w, h, facecolor="white", edgecolor="black", linewidth=1.5)
    ax.add_patch(e)
    ax.text(cx, cy, text, ha="center", va="center", fontsize=fs, fontweight="bold")


def process(ax, cx, cy, w, h, text, fs=8):
    x, y = cx - w / 2, cy - h / 2
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="square,pad=0.35", facecolor="white", edgecolor="black", lw=1.4))
    ax.text(cx, cy, text, ha="center", va="center", fontsize=fs)


def data_io(ax, cx, cy, w, h, text, fs=8):
    """Jajar genjang (input/output)."""
    dx = w * 0.12
    pts = [
        (cx - w / 2 + dx, cy - h / 2),
        (cx + w / 2 + dx, cy - h / 2),
        (cx + w / 2 - dx, cy + h / 2),
        (cx - w / 2 - dx, cy + h / 2),
    ]
    ax.add_patch(Polygon(pts, closed=True, facecolor="#f5f5f5", edgecolor="black", lw=1.4))
    ax.text(cx, cy, text, ha="center", va="center", fontsize=fs)


def database(ax, cx, cy, w, h, text, fs=8):
    """Silinder database."""
    ew, eh = w * 0.45, h * 0.22
    body_h = h - eh
    ax.add_patch(Ellipse((cx, cy + body_h / 2), w, eh, facecolor="#e8f5e9", edgecolor="black", lw=1.4))
    ax.add_patch(Rectangle((cx - w / 2, cy - body_h / 2), w, body_h, facecolor="#e8f5e9", edgecolor="black", lw=1.4))
    ax.add_patch(Ellipse((cx, cy - body_h / 2), w, eh, facecolor="#c8e6c9", edgecolor="black", lw=1.4))
    ax.text(cx, cy, text, ha="center", va="center", fontsize=fs, fontweight="bold")


def arrow(ax, x1, y1, x2, y2, color="#333", lw=1.3):
    ax.add_patch(FancyArrowPatch((x1, y1), (x2, y2), arrowstyle="-|>", mutation_scale=12, lw=lw, color=color))


def main():
    fig, ax = plt.subplots(figsize=(11, 16))
    ax.set_xlim(0, 11)
    ax.set_ylim(0, 20)
    ax.axis("off")
    ax.set_title(
        "Flowchart Sistem Monitoring & Penanganan Tanaman Cabai Rawit\n"
        "(ESP32 · Fuzzy Tahani · Supabase · MQTT · Dashboard Web)",
        fontsize=11, fontweight="bold", pad=14,
    )

    cx = 5.5

    # 1 Start
    terminal(ax, cx, 19.0, 2.2, 0.7, "Start")

    # 2 Tiga cabang pembacaan sensor (seperti contoh TA)
    y_read = 17.2
    xs = [2.2, 5.5, 8.8]
    labels_proc = [
        "Pembacaan\nSensor pH Tanah\n(DMS + ADC)",
        "Pembacaan\nSensor DHT22\n(Suhu & RH)",
        "Pembacaan\nSensor\nSoil Moisture",
    ]
    labels_data = ["Nilai pH", "Nilai Suhu\n& Kelembaban", "Nilai Kelembaban\nTanah (%)"]

    for x, lp, ld in zip(xs, labels_proc, labels_data):
        arrow(ax, cx, 18.65, x, 17.55)
        process(ax, x, y_read, 2.5, 0.95, lp, fs=7.5)
        arrow(ax, x, 16.7, x, 16.05)
        data_io(ax, x, 15.6, 2.3, 0.75, ld, fs=7.5)

    # 3 Gabung → Fuzzy Tahani (Mamdani)
    y_fuzzy = 13.8
    for x in xs:
        arrow(ax, x, 15.2, cx, 14.35)
    process(ax, cx, y_fuzzy, 4.8, 1.0, "Pengolahan Data\nFuzzy Tahani (3 jalur)", fs=9)
    arrow(ax, cx, 13.3, cx, 12.65)
    data_io(ax, cx, 12.0, 4.6, 0.85, "Skor fuzzy_suhu · fuzzy_soil · fuzzy_ph\n(centroid Mamdani)", fs=7.5)

    # 4 Kendali aktuator
    arrow(ax, cx, 11.55, cx, 10.95)
    process(ax, cx, 10.35, 5.0, 1.15, "Kendali Aktuator\n(Servo paranet · Relay air · Relay pH)", fs=8.5)
    arrow(ax, cx, 9.75, cx, 9.15)
    data_io(ax, cx, 8.55, 4.8, 0.8, "Status paranet / air / pH", fs=8)

    # 5 Kirim data (dua jalur paralel seperti cabang dari database di contoh)
    y_send = 7.35
    arrow(ax, cx, 8.15, cx, 7.85)
    # node kecil penggabung
    ax.plot(cx, 7.85, "ko", ms=6)
    arrow(ax, cx, 7.85, 2.8, y_send + 0.5)
    arrow(ax, cx, 7.85, 8.2, y_send + 0.5)

    process(ax, 2.8, y_send, 3.6, 0.95, "Proses Pengiriman\nke Database Supabase\n(REST HTTPS)", fs=7.5)
    process(ax, 8.2, y_send, 3.6, 0.95, "Proses Publish\nke Broker MQTT\n(EMQX)", fs=7.5)

    arrow(ax, 2.8, 6.85, 2.8, 6.35)
    database(ax, 2.8, 5.85, 2.8, 1.1, "Database\nSupabase")

    arrow(ax, 8.2, 6.85, 8.2, 6.35)
    data_io(ax, 8.2, 5.85, 3.0, 0.75, "Topik\npertanian/sensor", fs=7.5)

    # 6 Tampilan dashboard (dua cabang dari DB & MQTT)
    arrow(ax, 2.8, 5.3, 2.8, 4.75)
    process(ax, 2.8, 4.15, 4.0, 0.95, "Menampilkan Riwayat Data\npada Dashboard Web\n(SELECT Supabase)", fs=7.5)

    arrow(ax, 8.2, 5.45, 8.2, 4.75)
    process(ax, 8.2, 4.15, 4.0, 0.95, "Menampilkan Data Real-time\npada Dashboard Web\n(Subscribe MQTT)", fs=7.5)

    # 7 Loop monitoring
    arrow(ax, 2.8, 3.65, cx, 2.5)
    arrow(ax, 8.2, 3.65, cx, 2.5)
    process(ax, cx, 1.85, 4.5, 0.85, "Jeda & kembali ke pembacaan sensor\n(loop berkelanjutan)", fs=8)
    arrow(ax, cx, 1.4, cx, 19.35, color="#666")
    ax.annotate("", xy=(cx, 19.35), xytext=(cx, 1.4),
                arrowprops=dict(arrowstyle="-|>", color="#888", lw=1.0, connectionstyle="arc3,rad=0.35"))

    # Catatan
    ax.text(
        0.2, 0.35,
        "Perbedaan vs contoh air: sensor tanah+udara (bukan turbiditas); Fuzzy Tahani (Mamdani, centroid);\n"
        "dashboard web (bukan LCD/Telegram). Fuzzy & aktuator sebelum pengiriman cloud.",
        fontsize=7, va="bottom", style="italic", color="#444",
    )

    out = "flowchart_sistem.png"
    plt.tight_layout()
    plt.savefig(out, dpi=200, bbox_inches="tight", facecolor="white")
    print(f"Disimpan: {out}")


if __name__ == "__main__":
    import matplotlib
    matplotlib.use("Agg")
    main()
