# Pengkodean Tampilan Web Dashboard

---

## 1. Library dan Konfigurasi

```html
<script src="https://unpkg.com/mqtt/dist/mqtt.min.js"></script>
```
Memuat library MQTT.js dari CDN. Memungkinkan browser terhubung ke
broker MQTT menggunakan WebSocket sehingga dashboard menerima data
sensor secara real-time tanpa perlu reload halaman.

---

```js
const MQTT_HOST  = 'wss://n01d3130.ala.asia-southeast1.emqxsl.com:8084/mqtt';
const TOPIC_SUB  = 'pertanian/sensor';
const TOPIC_CTRL = 'pertanian/kontrol';
const SB_URL     = 'https://sptomqebtvclfebaktof.supabase.co';
const SB_KEY     = 'sb_publishable_jqEF4hY0nK0Bu0BkKK2ayQ_eW_8fh_u';
const REFRESH_SEC = 30;
```

**`MQTT_HOST`** — Alamat broker EMQX Cloud menggunakan protokol `wss://`
(WebSocket Secure) port 8084. Browser tidak bisa menggunakan TCP
langsung sehingga MQTT dikemas dalam WebSocket.

**`TOPIC_SUB`** — Topik untuk menerima data sensor dari ESP32 secara
real-time.

**`TOPIC_CTRL`** — Topik untuk mengirim perintah kontrol manual ke ESP32.

**`SB_URL` dan `SB_KEY`** — Alamat dan API key Supabase untuk mengambil
riwayat data dari database melalui REST API.

**`REFRESH_SEC` (30)** — Interval auto-refresh tabel riwayat setiap
30 detik.

---

## 2. Koneksi MQTT Real-Time

```js
function initMQTT() {
  const clientId = 'dashboard-' +
    Math.random().toString(16).slice(2, 8);
  client = mqtt.connect(MQTT_HOST, {
    clientId, username: MQTT_USER,
    password: MQTT_PASS, reconnectPeriod: 3000,
  });
  client.on('connect', () => {
    setConn(true);
    client.subscribe(TOPIC_SUB, { qos: 1 });
  });
  client.on('message', (topic, payload) => {
    updateDashboard(JSON.parse(payload.toString()));
  });
}
```

**`mqtt.connect()`** — Membuka koneksi WebSocket ke broker dengan
kredensial autentikasi.

**`reconnectPeriod: 3000`** — Jika koneksi terputus, browser otomatis
mencoba menghubungkan kembali setiap 3 detik tanpa perlu reload.

**`client.on('connect')`** — Callback saat koneksi berhasil. Langsung
subscribe ke topik sensor dengan QoS 1 agar tidak ada data terlewat.

**`client.on('message')`** — Callback setiap pesan baru masuk dari ESP32.
Payload JSON di-parse lalu diteruskan ke `updateDashboard()`.

---

## 3. Update Kartu Sensor dan Relay

```js
function updateDashboard(d) {
  setText('val-suhu', parseFloat(d.temperature).toFixed(1));
  setText('val-ph',   parseFloat(d.ph).toFixed(2));
  setBadge('badge-suhu',  d.status_suhu);
  setBadge('badge-tanah', d.status_tanah);
  setBadge('badge-ph',    d.status_ph);
  setMuBar('mu-suhu-bar', 'mu-suhu-txt',
           parseFloat(d.fuzzy_suhu));
  setRelayInd('ri-kipas',
           parseFloat(d.relay_kipas) >= 1.0);
  lastDataTime = Date.now();
}
```

**`setText()`** — Memperbarui nilai teks elemen HTML (kartu sensor) dengan
data terbaru dari payload MQTT tanpa reload halaman.

**`setBadge()`** — Memperbarui label himpunan fuzzy (Rendah/Sedang/Tinggi,
Kering/Lembab/Basah, Asam/Normal/Basa) beserta warna badge sesuai kondisi.

**`setMuBar()`** — Memperbarui progress bar derajat keanggotaan fuzzy
(0–100%) yang ditampilkan di bawah setiap kartu sensor.

**`setRelayInd()`** — Memperbarui indikator status relay ON/OFF. Nilai
float 1.0/0.0 dari MQTT dikonversi ke boolean dengan `>= 1.0`.

**`lastDataTime = Date.now()`** — Mencatat waktu terakhir data diterima
untuk digunakan oleh watchdog pendeteksi koneksi terputus.

---

## 4. Watchdog Deteksi ESP32 Terputus

```js
const DATA_TIMEOUT_SEC = 60;
let   lastDataTime     = 0;

setInterval(() => {
  if (lastDataTime === 0) return;
  const selisih = Math.floor(
    (Date.now() - lastDataTime) / 1000);
  if (selisih >= DATA_TIMEOUT_SEC) {
    setWifiStatus(false, '–', '–', '');
    document.getElementById('last-update')
      .textContent = 'Terputus sejak '
                     + selisih + ' detik lalu';
  }
}, 5000);
```

Berjalan setiap 5 detik. Jika tidak ada data MQTT masuk selama
60 detik, dashboard menampilkan status WiFi merah dan pesan
"Terputus sejak X detik lalu" di header.

---

## 5. Ambil Riwayat dari Supabase

```js
async function muatRiwayat() {
  const url = `${SB_URL}/rest/v1/${SB_TABLE}` +
    `?select=updated_at,temperature,humidity,soil,` +
    `ph,fuzzy_suhu,fuzzy_soil,fuzzy_ph,` +
    `relay_kipas,relay_air,relay_ph` +
    `&order=id.desc&limit=60`;

  const res = await fetch(url, {
    headers: {
      'apikey':        SB_KEY,
      'Authorization': 'Bearer ' + SB_KEY,
    }
  });
  const rows = await res.json();
  renderTabel(rows);
}
```

**`async function`** — Fungsi asynchronous agar proses fetch tidak
memblokir tampilan halaman.

**`?select=...`** — Memilih hanya kolom yang dibutuhkan agar data
lebih ringan.

**`&order=id.desc`** — Mengurutkan dari ID terbesar sehingga data
terbaru tampil di baris paling atas.

**`&limit=60`** — Membatasi hasil 60 baris terakhir.

**`fetch(url, { headers })`** — HTTP GET ke Supabase REST API dengan
header `apikey` dan `Authorization Bearer` untuk autentikasi.

---

## 6. Auto-Refresh Tabel

```js
let countdown = REFRESH_SEC;

setInterval(() => {
  countdown--;
  if (countdown <= 0) {
    countdown = REFRESH_SEC;
    muatRiwayat();
  }
  document.getElementById('refresh-countdown')
    .textContent = 'Refresh: ' + countdown + 's';
}, 1000);
```

Countdown mundur dari 30 detik. Saat mencapai 0, `muatRiwayat()`
dipanggil otomatis untuk mengambil data terbaru dari Supabase.
Countdown dan tabel diperbarui tanpa reload halaman.

---

## 7. Kontrol Manual

```js
function toggleManualMode(isManual) {
  isManualMode = isManual;
  _updateManualUI(isManual);
  if (!isManual) _publishKontrol({ manual: false });
}

function toggleAktuator(key) {
  manualState[key] = !manualState[key];
}

function kirimKontrol() {
  _publishKontrol({
    manual: true,
    kipas:     manualState.kipas,
    pompa_air: manualState.pompa_air,
    pompa_ph:  manualState.pompa_ph,
  });
}

function _publishKontrol(payload) {
  client.publish(TOPIC_CTRL,
    JSON.stringify(payload), { qos: 1 });
}
```

**`toggleManualMode()`** — Dipanggil saat switch Otomatis/Manual diubah.
Jika kembali ke otomatis, langsung kirim `{manual: false}` ke ESP32.

**`toggleAktuator()`** — Toggle state ON/OFF tiap tombol relay individual
di tampilan dashboard.

**`kirimKontrol()`** — Mengirim perintah JSON ke ESP32 via MQTT berisi
status masing-masing relay yang diinginkan.

**`_publishKontrol()`** — Helper yang memanggil `client.publish()` ke
topik `pertanian/kontrol`.

---

## 8. Inisialisasi saat Halaman Dibuka

```js
initMQTT();
muatRiwayat();
```

**`initMQTT()`** — Dipanggil pertama kali saat halaman dibuka untuk
membuka koneksi WebSocket ke broker MQTT dan mulai menerima data
real-time dari ESP32.

**`muatRiwayat()`** — Dipanggil langsung saat halaman dibuka agar tabel
riwayat langsung terisi tanpa menunggu countdown pertama selesai.

---

*File ini menjelaskan implementasi tampilan web dashboard pada sistem
monitoring greenhouse cabai rawit.*
