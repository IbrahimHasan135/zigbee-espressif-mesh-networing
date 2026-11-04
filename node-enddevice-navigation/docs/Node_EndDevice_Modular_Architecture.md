# Modular Firmware Architecture: Zigbee End Device Navigation

Dokumen ini merancang struktur firmware untuk `node-enddevice-navigation` dengan target:

- Board: ESP32-C6
- Framework: ESP-IDF 5.5.4
- Zigbee SDK: Espressif Zigbee SDK v2 (`espressif/esp-zigbee-lib` v2.x)
- Role Zigbee: Sleepy End Device / Zigbee End Device
- Pola firmware: modular, OOP untuk driver dan service, FreeRTOS orchestration di controller

Dokumen ini fokus ke struktur dan arsitektur. Implementasi kode firmware bisa dilakukan setelah struktur ini disetujui.

## 1. Prinsip Arsitektur

Firmware dibagi menjadi tiga lapisan utama:

1. `driver`
   - Class C++ yang menjadi pembungkus akses langsung ke ESP-IDF SDK dan ESP Zigbee SDK.
   - Driver boleh memanggil API seperti `esp_zb_*`, `esp_sleep_*`, `nvs_flash_*`, GPIO, timer, dan API ESP-IDF lain.
   - Driver tidak menjalankan loop bisnis sendiri.
   - Driver tidak membuat FreeRTOS task untuk workflow aplikasi.
   - Driver bersifat pasif: hanya bekerja ketika method-nya dipanggil controller/service atau ketika callback SDK perlu disalin cepat menjadi event ringan.

2. `service`
   - Class C++ yang berisi logika domain aplikasi.
   - Service mengakses method dan attribute object driver.
   - Service tidak memanggil SDK langsung.
   - Service tidak membuat FreeRTOS task.
   - Service bersifat pasif: hanya memproses data ketika dipanggil controller.

3. `controller`
   - Lapisan aktif yang mengatur lifecycle aplikasi.
   - Controller menginisialisasi driver, lalu menginisialisasi service, lalu menghubungkan object driver ke service.
   - Controller membuat FreeRTOS task, queue, timer, dan state machine.
   - Controller menentukan kapan ping dikirim, kapan listening window dibuka, kapan data RSSI diproses, dan kapan perangkat masuk sleep.

Dengan pola ini, alur aksesnya adalah:

```text
FreeRTOS Task / SDK Callback
        |
        v
Controller
        |
        v
Service
        |
        v
Driver
        |
        v
ESP-IDF SDK / ESP Zigbee SDK
```

## 2. Struktur Direktori yang Disarankan

Struktur project disarankan tetap mengikuti pola ESP-IDF component, tetapi `main/` dibuat modular:

```text
node-enddevice-navigation/
├── CMakeLists.txt
├── sdkconfig.defaults
├── docs/
│   ├── Node_EndDevice_Codex.md
│   └── Node_EndDevice_Modular_Architecture.md
└── main/
    ├── CMakeLists.txt
    ├── idf_component.yml
    ├── app_main.cpp
    ├── config/
    │   ├── app_config.h
    │   ├── zigbee_config.h
    │   └── navigation_config.h
    ├── common/
    │   ├── nav_types.h
    │   ├── app_error.h
    │   └── byte_utils.h
    ├── drivers/
    │   ├── zigbee/
    │   │   ├── zigbee_end_device_driver.hpp
    │   │   └── zigbee_end_device_driver.cpp
    │   ├── power/
    │   │   ├── power_driver.hpp
    │   │   └── power_driver.cpp
    │   └── storage/
    │       ├── storage_driver.hpp
    │       └── storage_driver.cpp
    ├── services/
    │   ├── navigation/
    │   │   ├── navigation_service.hpp
    │   │   └── navigation_service.cpp
    │   ├── positioning/
    │   │   ├── positioning_service.hpp
    │   │   └── positioning_service.cpp
    │   └── power/
    │       ├── power_service.hpp
    │       └── power_service.cpp
    └── controllers/
        ├── app_controller.hpp
        ├── app_controller.cpp
        ├── navigation_controller.hpp
        ├── navigation_controller.cpp
        ├── zigbee_controller.hpp
        └── zigbee_controller.cpp
```

Catatan:

- `main.c` sebaiknya diganti menjadi `app_main.cpp` agar driver dan service bisa dibuat sebagai class C++.
- Jika tetap ingin entry point C-compatible, `app_main.cpp` tetap boleh berisi `extern "C" void app_main(void)`.
- `sdkconfig.defaults` perlu disiapkan untuk target ESP32-C6 dan Zigbee End Device, misalnya konfigurasi `CONFIG_ZB_ZED=y` sesuai kebutuhan Espressif Zigbee SDK.
- `main/idf_component.yml` saat ini sudah memakai `espressif/esp-zigbee-lib: ^2.0.4`, tetapi versi ESP-IDF masih tertulis `>=4.1.0`. Untuk target desain ini, dependency IDF sebaiknya dikunci ke `>=5.5.4` atau range yang sesuai dengan environment build.

## 3. Tanggung Jawab Setiap Lapisan

### 3.1 Driver Layer

Driver adalah wrapper OOP untuk SDK. Driver tidak tahu alur bisnis indoor navigation secara lengkap.

Contoh driver:

```text
ZigbeeEndDeviceDriver
```

Tanggung jawab:

- Init Zigbee stack sebagai End Device.
- Konfigurasi endpoint, cluster, attribute, custom cluster navigation.
- Start Zigbee stack.
- Kirim custom command ping broadcast.
- Ambil metadata frame masuk seperti source address, RSSI/LQI, endpoint, cluster id, command id, dan payload.
- Menyediakan method pasif seperti:
  - `init()`
  - `start()`
  - `sendBroadcastPing()`
  - `decodeIncomingCommand(...)`
  - `allowSleepyEndDeviceMode(...)`

Tidak boleh:

- Membuat task navigasi.
- Menghitung posisi.
- Menentukan kapan device harus tidur berdasarkan workflow aplikasi.
- Menyimpan state cycle navigation tingkat aplikasi.

```text
PowerDriver
```

Tanggung jawab:

- Wrapper untuk light sleep / deep sleep.
- Konfigurasi wakeup source.
- Masuk sleep ketika diperintah service/controller.
- Membaca wakeup reason.

Tidak boleh:

- Menentukan sendiri kapan ping lifecycle selesai.
- Mengatur sequence navigation.

```text
StorageDriver
```

Tanggung jawab:

- Wrapper NVS atau storage lokal.
- Simpan dan baca konfigurasi node, calibration value, interval wakeup, atau last known state.

Tidak boleh:

- Mengambil keputusan navigasi.

### 3.2 Service Layer

Service adalah logic domain, tetapi tetap pasif. Service memakai object driver yang sudah disiapkan controller.

Contoh service:

```text
NavigationService
```

Tanggung jawab:

- Meminta `ZigbeeEndDeviceDriver` mengirim ping broadcast.
- Memvalidasi apakah frame masuk adalah response navigation.
- Parsing payload router:
  - `router_id`
  - `x`
  - `y`
  - `rssi`
- Mengumpulkan sample RSSI selama listening window.
- Menghasilkan list anchor/router response untuk positioning.
- Menyediakan method pasif seperti:
  - `beginCycle()`
  - `sendPing()`
  - `acceptFrame(...)`
  - `finishCycle()`
  - `getSamples()`

```text
PositioningService
```

Tanggung jawab:

- Mengubah RSSI menjadi estimasi jarak.
- Menjalankan trilateration atau weighted centroid.
- Menghasilkan estimasi lokasi node.
- Menyediakan method seperti:
  - `estimateDistanceFromRssi(...)`
  - `calculatePosition(...)`

```text
PowerService
```

Tanggung jawab:

- Menentukan durasi sleep berdasarkan hasil cycle.
- Meminta `PowerDriver` masuk light sleep / deep sleep.
- Membuat policy hemat daya, misalnya:
  - Jika join Zigbee belum selesai, jangan deep sleep.
  - Jika sample kurang, retry singkat.
  - Jika sample cukup, sleep panjang.

Service boleh menyimpan state domain, tetapi tidak boleh membuat task sendiri.

### 3.3 Controller Layer

Controller adalah bagian aktif firmware. Semua FreeRTOS task dibuat dan dikelola di sini.

Contoh controller:

```text
AppController
```

Tanggung jawab:

- Entry orchestration dari `app_main`.
- Membuat object driver.
- Membuat object service.
- Inject dependency driver ke service.
- Memulai controller lain.

```text
ZigbeeController
```

Tanggung jawab:

- Membuat task untuk Zigbee stack jika diperlukan.
- Register Zigbee callback ke ESP Zigbee SDK melalui driver.
- Menerima callback SDK, menyalinnya ke event kecil, lalu push ke FreeRTOS queue.
- Menjaga callback tetap cepat dan non-blocking.

```text
NavigationController
```

Tanggung jawab:

- Membuat task lifecycle navigation.
- Membuat queue untuk event frame masuk.
- Mengatur state:
  - boot
  - zigbee join
  - ping broadcast
  - listening window
  - process samples
  - publish/report result jika diperlukan
  - sleep
- Memanggil `NavigationService`, `PositioningService`, dan `PowerService`.

## 4. Pola Dependency Injection

Controller membuat object dalam urutan berikut:

```cpp
ZigbeeEndDeviceDriver zigbee_driver;
PowerDriver power_driver;
StorageDriver storage_driver;

NavigationService navigation_service(zigbee_driver);
PositioningService positioning_service;
PowerService power_service(power_driver, storage_driver);

NavigationController navigation_controller(
    navigation_service,
    positioning_service,
    power_service
);

ZigbeeController zigbee_controller(
    zigbee_driver,
    navigation_controller
);
```

Maknanya:

- Driver tidak membuat service.
- Service tidak membuat driver.
- Controller yang menyusun dependency antar object.
- Service menerima reference/pointer driver melalui constructor.
- Controller tetap pemilik lifecycle aplikasi.

## 5. Komunikasi Driver, Service, dan Controller

### 5.1 Alur Kirim Ping

```text
NavigationController task
        |
        | calls beginCycle()
        v
NavigationService
        |
        | calls sendBroadcastPing()
        v
ZigbeeEndDeviceDriver
        |
        | calls esp_zb_zcl_custom_cluster_cmd_req()
        v
ESP Zigbee SDK
```

Detail:

- Controller menentukan kapan cycle dimulai.
- Service menyiapkan konteks cycle dan meminta driver mengirim command.
- Driver hanya menerjemahkan request menjadi pemanggilan SDK.

### 5.2 Alur Terima Response RSSI

```text
ESP Zigbee SDK callback
        |
        v
ZigbeeController static callback
        |
        | calls driver.decodeIncomingCommand()
        v
ZigbeeEndDeviceDriver
        |
        | returns lightweight ZigbeeFrame
        v
ZigbeeController
        |
        | xQueueSend(..., timeout 0)
        v
NavigationController event task
        |
        | calls navigationService.acceptFrame(frame)
        v
NavigationService
        |
        | stores NavSample
        v
PositioningService calculatePosition(samples)
```

Callback SDK harus singkat:

- Tidak menjalankan trilateration.
- Tidak melakukan logging panjang.
- Tidak melakukan alokasi dinamis besar.
- Tidak menunggu mutex lama.
- Hanya copy metadata dan payload penting ke struct event, lalu kirim ke queue.

### 5.3 Alur Sleep

```text
NavigationController
        |
        | after processing samples
        v
PowerService
        |
        | decides sleep duration and mode
        v
PowerDriver
        |
        | calls ESP-IDF sleep API
        v
ESP-IDF Power Management
```

Power policy berada di service, tetapi eksekusi SDK tetap di driver.

## 6. FreeRTOS Task yang Disarankan

### 6.1 `zigbee_main_task`

Pemilik: `ZigbeeController`

Tanggung jawab:

- Menjalankan Zigbee stack loop sesuai pola ESP Zigbee SDK.
- Menginisialisasi Zigbee driver sebelum stack start.
- Menjaga semua panggilan Zigbee yang wajib berada dalam context task Zigbee tetap aman.

Catatan implementasi:

- ESP Zigbee SDK biasanya memiliki pola khusus untuk init, start, dan main loop.
- Semua akses Zigbee SDK yang perlu lock atau context khusus harus dipusatkan lewat driver/controller ini.

### 6.2 `navigation_cycle_task`

Pemilik: `NavigationController`

Tanggung jawab:

- Menunggu Zigbee join/network ready.
- Memulai navigation cycle.
- Memanggil `NavigationService::sendPing()`.
- Membuka listening window, misalnya 150 ms.
- Menutup cycle.
- Meminta `PositioningService` menghitung posisi.
- Meminta `PowerService` menentukan sleep/retry.

Contoh state:

```text
WAIT_NETWORK_READY
START_CYCLE
SEND_PING
LISTEN_WINDOW
PROCESS_SAMPLES
SLEEP_OR_RETRY
```

### 6.3 `navigation_event_task`

Pemilik: `NavigationController`

Tanggung jawab:

- Membaca queue event dari callback Zigbee.
- Meneruskan frame ke `NavigationService::acceptFrame(...)`.
- Tidak menghitung posisi berat di callback.

Task ini bisa digabung dengan `navigation_cycle_task` jika state machine dibuat rapi, tetapi untuk modularitas awal lebih jelas dipisah.

## 7. Data Model Internal

Data di `common/nav_types.h` disarankan seperti ini:

```cpp
struct ZigbeeFrame {
    uint16_t src_short_addr;
    uint8_t src_endpoint;
    uint16_t cluster_id;
    uint8_t command_id;
    int8_t rssi;
    uint8_t lqi;
    uint8_t payload[32];
    uint8_t payload_len;
};

struct NavAnchorSample {
    uint16_t router_id;
    float x;
    float y;
    int8_t rssi;
    uint8_t lqi;
    uint32_t received_at_ms;
};

struct Position2D {
    float x;
    float y;
    float confidence;
};
```

Untuk tahap awal, payload response router bisa dibuat sederhana:

```text
byte 0..1 : router_id, uint16 big-endian
byte 2..3 : x, uint16 big-endian
byte 4..5 : y, uint16 big-endian
```

RSSI tidak perlu dikirim di payload karena diambil dari metadata frame Zigbee yang diterima node.

## 8. Custom Zigbee Contract Awal

Cluster navigation:

```text
CUSTOM_NAV_CLUSTER_ID = 0xFF01
```

Command:

```text
CMD_PING_REQ = 0x01
CMD_PING_RSP = 0x02
```

Ping request dari node ke router:

```text
Destination : broadcast ke router/RxOnWhenIdle target sesuai kebutuhan stack
Cluster     : 0xFF01
Command     : 0x01
Payload     : kosong atau berisi sequence_id
```

Ping response dari router ke node:

```text
Destination : unicast ke short address node
Cluster     : 0xFF01
Command     : 0x02
Payload     : router_id + x + y
Metadata    : RSSI/LQI dari received frame
```

Disarankan menambahkan `sequence_id` agar response yang masuk bisa dikorelasikan dengan cycle aktif:

```text
PING_REQ payload:
byte 0..1 : sequence_id

PING_RSP payload:
byte 0..1 : sequence_id
byte 2..3 : router_id
byte 4..5 : x
byte 6..7 : y
```

Jika `sequence_id` dipakai, `NavigationService::acceptFrame(...)` hanya menerima response yang sequence-nya sama dengan cycle aktif.

## 9. Lifecycle Boot sampai Sleep

Urutan runtime yang disarankan:

```text
app_main()
  |
  v
AppController::init()
  |
  +-- init StorageDriver
  +-- init PowerDriver
  +-- init ZigbeeEndDeviceDriver
  +-- create services with driver references
  +-- create queues
  +-- register Zigbee callback
  |
  v
AppController::start()
  |
  +-- start ZigbeeController task
  +-- start NavigationController tasks
  |
  v
Zigbee join complete
  |
  v
Navigation cycle
  |
  +-- send ping broadcast
  +-- listen 150 ms
  +-- collect responses from queue
  +-- calculate position
  +-- decide sleep/retry
  |
  v
Sleep
```

## 10. Boundary yang Harus Dijaga Saat Implementasi

Aturan penting agar modularitas tidak bocor:

- File di `drivers/` boleh include header ESP-IDF dan ESP Zigbee SDK.
- File di `services/` sebaiknya tidak include header `esp_zb_*` langsung.
- File di `controllers/` boleh include FreeRTOS headers.
- Service tidak boleh membuat task, queue, timer, atau semaphore.
- Driver tidak boleh menjalankan state machine aplikasi.
- Controller boleh memiliki queue dan task.
- Callback Zigbee tidak boleh menghitung trilateration.
- Semua parsing payload navigation sebaiknya berada di service atau helper `common/`, bukan di callback besar.
- Semua detail SDK tetap dikurung di driver agar saat API Espressif berubah, dampaknya terbatas.

## 11. Rencana Implementasi Bertahap

### Tahap 1: Struktur Skeleton

- Ubah `main.c` menjadi `app_main.cpp`.
- Tambahkan folder `drivers/`, `services/`, `controllers/`, `common/`, dan `config/`.
- Update `main/CMakeLists.txt` agar compile semua `.cpp`.
- Tambahkan `sdkconfig.defaults` untuk ESP32-C6 dan ZED.

### Tahap 2: Driver Zigbee Minimal

- Implement `ZigbeeEndDeviceDriver::init()`.
- Implement endpoint dan custom cluster `0xFF01`.
- Implement `sendBroadcastPing(sequence_id)`.
- Implement decode frame masuk menjadi `ZigbeeFrame`.

### Tahap 3: Controller FreeRTOS

- Implement `ZigbeeController`.
- Implement queue event Zigbee.
- Implement `NavigationController` dengan:
  - `navigation_cycle_task`
  - `navigation_event_task`

### Tahap 4: Navigation Service

- Implement cycle state pasif.
- Implement sample buffer.
- Implement validasi cluster, command, payload length, dan `sequence_id`.

### Tahap 5: Positioning Service

- Mulai dari weighted centroid agar sederhana.
- Lanjutkan ke trilateration jika anchor/sample sudah stabil.

### Tahap 6: Power Service

- Implement policy retry/sleep.
- Integrasikan light sleep atau deep sleep.
- Pastikan Zigbee rejoin/reconnect behavior tervalidasi di hardware.

## 12. Ringkasan Desain

Desain ini membuat firmware punya batas yang jelas:

- Driver adalah class pasif yang membungkus SDK.
- Service adalah class pasif yang memakai driver dan menyimpan logika domain.
- Controller adalah satu-satunya lapisan yang aktif dengan FreeRTOS task dan queue.
- Callback Zigbee hanya menjadi jalur masuk event cepat.
- Perhitungan posisi dan policy sleep tidak masuk ke callback.
- Struktur ini cocok untuk pengembangan bertahap karena Zigbee SDK, algoritma positioning, dan power policy bisa diuji serta diganti tanpa mencampur seluruh kode di `main.c`.
