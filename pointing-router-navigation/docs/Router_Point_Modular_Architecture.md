# Modular Firmware Architecture: Zigbee Router Point Navigation

Dokumen ini merancang struktur firmware untuk `pointing-router-navigation` dengan target:

- Board: ESP32-C6
- Framework: ESP-IDF 5.5.4
- Zigbee SDK: Espressif Zigbee SDK v2 (`espressif/esp-zigbee-lib` v2.x)
- Role Zigbee: Zigbee Router / Router Point statis
- Power mode: mains-powered, `RxOnWhenIdle = true`
- Pola firmware: modular, OOP untuk driver dan service, FreeRTOS orchestration di controller

Dokumen ini fokus ke struktur dan architecture firmware. Implementasi source code bisa dilakukan setelah rancangan ini disetujui.

## 1. Peran Firmware Router Point

Router Point adalah node statis untuk sistem indoor navigation. Tugas utamanya bukan menghitung posisi, tetapi menjadi anchor/satelit referensi yang:

1. Join ke jaringan Zigbee sebagai router.
2. Selalu siap menerima frame karena perangkat dicolok ke listrik.
3. Mendengarkan `PING_REQ` dari End Device.
4. Membalas cepat dengan `PING_RSP` unicast ke pengirim.
5. Mengirim payload koordinat statis miliknya, misalnya `router_id`, `x`, dan `y`.

Router Point tidak melakukan active scanning, tidak membuat cycle navigasi sendiri, dan tidak menghitung posisi node bergerak.

## 2. Prinsip Arsitektur

Firmware dibagi menjadi tiga lapisan utama:

1. `driver`
   - Class C++ yang membungkus akses langsung ke ESP-IDF SDK dan ESP Zigbee SDK.
   - Driver boleh memanggil API seperti `esp_zb_*`, `nvs_flash_*`, GPIO, timer, log, dan API ESP-IDF lain.
   - Driver bersifat pasif.
   - Driver tidak membuat FreeRTOS task untuk workflow aplikasi.
   - Driver tidak menyimpan logika bisnis navigation router.

2. `service`
   - Class C++ yang menyimpan logika domain router point.
   - Service mengakses method dan attribute dari object driver.
   - Service tidak memanggil SDK langsung.
   - Service bersifat pasif.
   - Service tidak membuat task, queue, timer, atau semaphore.

3. `controller`
   - Layer aktif yang mengatur lifecycle firmware.
   - Controller membuat object driver.
   - Controller membuat object service.
   - Controller melakukan dependency injection dari driver ke service.
   - Controller membuat FreeRTOS task dan queue.
   - Controller menerima event dari Zigbee callback lalu memanggil service untuk validasi dan response.

Alur akses utama:

```text
FreeRTOS Task / Zigbee SDK Callback
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

Aturan penting:

- Driver adalah satu-satunya tempat yang langsung bicara ke SDK.
- Service adalah tempat domain logic.
- Controller adalah satu-satunya tempat workflow aktif dan FreeRTOS hidup.

## 3. Struktur Direktori yang Disarankan

Struktur project disarankan tetap mengikuti pola ESP-IDF component, tetapi isi `main/` dibuat modular:

```text
pointing-router-navigation/
|-- CMakeLists.txt
|-- sdkconfig.defaults
|-- docs/
|   |-- Router_Point_Codex.md
|   `-- Router_Point_Modular_Architecture.md
`-- main/
    |-- CMakeLists.txt
    |-- idf_component.yml
    |-- app_main.cpp
    |-- config/
    |   |-- app_config.h
    |   |-- zigbee_config.h
    |   `-- router_point_config.h
    |-- common/
    |   |-- nav_protocol.h
    |   |-- nav_types.h
    |   |-- app_error.h
    |   `-- byte_utils.h
    |-- drivers/
    |   |-- zigbee/
    |   |   |-- zigbee_router_driver.hpp
    |   |   `-- zigbee_router_driver.cpp
    |   |-- storage/
    |   |   |-- storage_driver.hpp
    |   |   `-- storage_driver.cpp
    |   `-- status/
    |       |-- status_led_driver.hpp
    |       `-- status_led_driver.cpp
    |-- services/
    |   |-- router_point/
    |   |   |-- router_point_service.hpp
    |   |   `-- router_point_service.cpp
    |   |-- navigation_protocol/
    |   |   |-- navigation_protocol_service.hpp
    |   |   `-- navigation_protocol_service.cpp
    |   `-- config/
    |       |-- router_config_service.hpp
    |       `-- router_config_service.cpp
    `-- controllers/
        |-- app_controller.hpp
        |-- app_controller.cpp
        |-- zigbee_controller.hpp
        |-- zigbee_controller.cpp
        |-- router_point_controller.hpp
        `-- router_point_controller.cpp
```

Catatan:

- `main.c` sebaiknya diganti menjadi `app_main.cpp` agar firmware bisa memakai class C++.
- Entry point tetap menggunakan `extern "C" void app_main(void)` agar sesuai kontrak ESP-IDF.
- `sdkconfig.defaults` perlu menargetkan ESP32-C6 dan Zigbee Router, misalnya `CONFIG_ZB_ZCZR=y` sesuai kebutuhan SDK.
- `main/idf_component.yml` saat ini sudah memakai `espressif/esp-zigbee-lib: ^2.0.4`.
- Karena target framework adalah ESP-IDF 5.5.4, dependency IDF di manifest sebaiknya dinaikkan dari `>=4.1.0` menjadi range yang sesuai ketika tahap implementasi dimulai.

## 4. Driver Layer

Driver adalah wrapper OOP untuk SDK dan hardware. Driver tidak tahu flow lengkap aplikasi.

### 4.1 `ZigbeeRouterDriver`

Tanggung jawab:

- Inisialisasi ESP Zigbee stack sebagai Router.
- Konfigurasi network role Zigbee Router.
- Konfigurasi endpoint navigation.
- Konfigurasi custom cluster navigation `0xFF01`.
- Register handler/callback melalui SDK.
- Start Zigbee stack.
- Decode raw callback message menjadi struct internal yang ringan.
- Mengirim custom command unicast `PING_RSP`.

Contoh method:

```cpp
class ZigbeeRouterDriver {
public:
    esp_err_t init();
    esp_err_t start();
    esp_err_t registerCallbacks();
    esp_err_t sendPingResponse(uint16_t dst_short_addr, const uint8_t *payload, uint8_t payload_len);
    bool decodeIncomingCommand(const void *sdk_message, ZigbeeFrame &out_frame);
    bool isNetworkReady() const;
};
```

Yang tidak boleh dilakukan driver:

- Membuat task FreeRTOS aplikasi.
- Menjalankan state machine router point.
- Menentukan apakah sebuah payload valid secara domain.
- Membaca koordinat bisnis lalu memutuskan response sendiri.

Driver hanya menerjemahkan perintah object lain menjadi pemanggilan SDK.

### 4.2 `StorageDriver`

Tanggung jawab:

- Wrapper NVS.
- Membaca dan menulis konfigurasi router point.
- Menyimpan `router_id`, koordinat `x`, koordinat `y`, dan opsi calibration.

Contoh method:

```cpp
class StorageDriver {
public:
    esp_err_t init();
    esp_err_t readRouterConfig(RouterPointConfig &out_config);
    esp_err_t writeRouterConfig(const RouterPointConfig &config);
};
```

Yang tidak boleh dilakukan driver:

- Memvalidasi domain navigation secara lengkap.
- Mengirim frame Zigbee.
- Mengatur task atau queue.

### 4.3 `StatusLedDriver`

Opsional, tetapi berguna saat debugging hardware.

Tanggung jawab:

- Wrapper GPIO LED.
- Menyalakan indikator boot, join, receive ping, atau send response.

Contoh method:

```cpp
class StatusLedDriver {
public:
    esp_err_t init();
    void setBooting();
    void setJoined();
    void pulsePingReceived();
    void pulseResponseSent();
};
```

Driver LED tetap pasif. Controller/service yang menentukan kapan indicator dipanggil.

## 5. Service Layer

Service berisi logika domain. Service memakai driver, tetapi tidak tahu detail SDK.

### 5.1 `RouterConfigService`

Tanggung jawab:

- Load konfigurasi router dari `StorageDriver`.
- Jika NVS belum berisi konfigurasi, gunakan default compile-time dari `router_point_config.h`.
- Menyediakan akses pasif ke:
  - `router_id`
  - `x`
  - `y`
  - response enable flag

Contoh method:

```cpp
class RouterConfigService {
public:
    explicit RouterConfigService(StorageDriver &storage_driver);

    esp_err_t init();
    const RouterPointConfig &getConfig() const;
    esp_err_t updateConfig(const RouterPointConfig &config);
};
```

### 5.2 `NavigationProtocolService`

Tanggung jawab:

- Menyimpan definisi protocol navigation.
- Memvalidasi cluster id dan command id.
- Decode `PING_REQ`.
- Encode `PING_RSP`.
- Menangani `sequence_id` jika protocol memakainya.

Contoh method:

```cpp
class NavigationProtocolService {
public:
    bool isPingRequest(const ZigbeeFrame &frame) const;
    bool decodePingRequest(const ZigbeeFrame &frame, PingRequest &out_request) const;
    bool encodePingResponse(const PingRequest &request,
                            const RouterPointConfig &config,
                            PingResponsePayload &out_payload) const;
};
```

Service ini tidak mengirim frame. Ia hanya validasi dan encode/decode payload.

### 5.3 `RouterPointService`

Tanggung jawab:

- Menggabungkan logic config dan protocol untuk memproses ping.
- Menentukan apakah ping perlu dibalas.
- Membuat payload response dengan koordinat router.
- Meminta `ZigbeeRouterDriver` mengirim response.

Contoh method:

```cpp
class RouterPointService {
public:
    RouterPointService(ZigbeeRouterDriver &zigbee_driver,
                       RouterConfigService &config_service,
                       NavigationProtocolService &protocol_service);

    esp_err_t handleIncomingFrame(const ZigbeeFrame &frame);
};
```

Alur di dalam service:

```text
handleIncomingFrame(frame)
  |
  +-- protocol_service.isPingRequest(frame)
  +-- protocol_service.decodePingRequest(frame)
  +-- config_service.getConfig()
  +-- protocol_service.encodePingResponse(...)
  `-- zigbee_driver.sendPingResponse(...)
```

Service ini pasif karena hanya bekerja saat controller memanggil `handleIncomingFrame(...)`.

## 6. Controller Layer

Controller adalah bagian aktif firmware dan pemilik FreeRTOS.

### 6.1 `AppController`

Tanggung jawab:

- Entry orchestration dari `app_main`.
- Membuat object driver.
- Membuat object service.
- Melakukan dependency injection.
- Membuat queue global internal controller.
- Memulai controller Zigbee dan Router Point.

Contoh dependency construction:

```cpp
StorageDriver storage_driver;
StatusLedDriver status_led_driver;
ZigbeeRouterDriver zigbee_driver;

RouterConfigService config_service(storage_driver);
NavigationProtocolService protocol_service;

RouterPointService router_point_service(
    zigbee_driver,
    config_service,
    protocol_service
);

RouterPointController router_point_controller(
    router_point_service,
    status_led_driver
);

ZigbeeController zigbee_controller(
    zigbee_driver,
    router_point_controller
);
```

Aturan:

- Driver tidak membuat service.
- Service tidak membuat driver.
- Controller yang menyusun semua dependency.

### 6.2 `ZigbeeController`

Tanggung jawab:

- Mengatur init dan start Zigbee stack.
- Menjalankan task Zigbee sesuai pola ESP Zigbee SDK.
- Register callback SDK.
- Menerima raw callback, meminta driver decode raw message, lalu push event ke queue controller.

Callback Zigbee harus cepat:

```text
ESP Zigbee SDK callback
  |
  +-- ZigbeeController static callback
  +-- zigbee_driver.decodeIncomingCommand(...)
  +-- xQueueSend(router_event_queue, &event, 0)
  `-- return ESP_OK
```

Callback tidak boleh:

- Membaca NVS.
- Logging panjang.
- Delay.
- Trilateration.
- Alokasi dinamis besar.
- Menunggu lock lama.

### 6.3 `RouterPointController`

Tanggung jawab:

- Membuat `router_event_task`.
- Membaca queue event yang berisi frame Zigbee masuk.
- Memanggil `RouterPointService::handleIncomingFrame(...)`.
- Mengatur indikator LED atau diagnostic ringan.
- Menjaga semua response logic berjalan di task, bukan callback SDK langsung.

Contoh alur:

```text
router_event_task
  |
  +-- wait frame from queue
  +-- status_led_driver.pulsePingReceived()
  +-- router_point_service.handleIncomingFrame(frame)
  `-- status_led_driver.pulseResponseSent()
```

## 7. FreeRTOS Task yang Disarankan

### 7.1 `zigbee_main_task`

Pemilik: `ZigbeeController`

Tanggung jawab:

- Init Zigbee stack.
- Start Zigbee network sebagai router.
- Menjaga Zigbee stack loop.
- Menangani signal seperti join/rejoin/network ready.

Catatan:

- ESP Zigbee SDK punya pola init/start/main loop sendiri.
- Jika API Zigbee butuh lock atau context tertentu, pusatkan akses itu di driver/controller.

### 7.2 `router_event_task`

Pemilik: `RouterPointController`

Tanggung jawab:

- Membaca frame dari queue.
- Memvalidasi event via service.
- Mengirim response lewat service.

Task ini adalah jalur utama domain logic router.

### 7.3 `diagnostic_task`

Opsional.

Tanggung jawab:

- Periodic log status ringan.
- Menampilkan network ready, jumlah ping diterima, jumlah response terkirim, dan error terakhir.
- Tidak wajib untuk firmware final.

Jika dibuat, diagnostic task tetap berada di controller, bukan service.

## 8. Data Model Internal

File `common/nav_types.h` disarankan berisi struct internal yang bebas dari tipe SDK.

```cpp
struct ZigbeeFrame {
    uint16_t src_short_addr;
    uint16_t dst_short_addr;
    uint8_t src_endpoint;
    uint8_t dst_endpoint;
    uint16_t cluster_id;
    uint8_t command_id;
    int8_t rssi;
    uint8_t lqi;
    uint8_t payload[32];
    uint8_t payload_len;
};

struct RouterPointConfig {
    uint16_t router_id;
    uint16_t x_cm;
    uint16_t y_cm;
    bool response_enabled;
};

struct PingRequest {
    uint16_t sequence_id;
    uint16_t sender_short_addr;
    bool has_sequence_id;
};

struct PingResponsePayload {
    uint8_t bytes[16];
    uint8_t len;
};
```

Tujuan struct internal:

- Service tidak tergantung pada tipe `esp_zb_*`.
- Testing logic service lebih mudah.
- Perubahan API SDK hanya memengaruhi driver.

## 9. Custom Zigbee Contract Awal

Cluster navigation:

```text
CUSTOM_NAV_CLUSTER_ID = 0xFF01
```

Command:

```text
CMD_PING_REQ = 0x01
CMD_PING_RSP = 0x02
```

### 9.1 Ping Request dari End Device

Minimal payload:

```text
Destination : broadcast
Cluster     : 0xFF01
Command     : 0x01
Payload     : kosong
```

Payload dengan sequence:

```text
byte 0..1 : sequence_id, uint16 big-endian
```

Sequence disarankan agar Router Point bisa mengembalikan ID cycle yang sama kepada End Device.

### 9.2 Ping Response dari Router Point

Minimal payload sesuai dokumen awal:

```text
Destination : unicast ke short address pengirim
Cluster     : 0xFF01
Command     : 0x02
Payload     : x + y

byte 0..1 : x_cm, uint16 big-endian
byte 2..3 : y_cm, uint16 big-endian
```

Payload yang disarankan untuk versi modular:

```text
byte 0..1 : sequence_id, uint16 big-endian
byte 2..3 : router_id, uint16 big-endian
byte 4..5 : x_cm, uint16 big-endian
byte 6..7 : y_cm, uint16 big-endian
```

Alasan `router_id` dan `sequence_id` disarankan:

- End Device bisa tahu response berasal dari anchor mana.
- End Device bisa mengabaikan response lama yang masuk di luar cycle aktif.
- Data RSSI tetap tidak perlu dikirim di payload karena RSSI dibaca dari metadata frame yang diterima End Device.

## 10. Alur Runtime

Urutan boot sampai router siap:

```text
app_main()
  |
  v
AppController::init()
  |
  +-- init StorageDriver
  +-- init StatusLedDriver
  +-- init ZigbeeRouterDriver
  +-- init RouterConfigService
  +-- create NavigationProtocolService
  +-- create RouterPointService
  +-- create event queue
  +-- register Zigbee callback
  |
  v
AppController::start()
  |
  +-- start zigbee_main_task
  +-- start router_event_task
  |
  v
Zigbee network ready as router
  |
  v
Wait for PING_REQ
```

Urutan saat ada ping:

```text
End Device sends PING_REQ broadcast
  |
  v
ESP Zigbee SDK receives custom cluster command
  |
  v
ZigbeeController callback copies frame to queue
  |
  v
RouterPointController router_event_task receives frame
  |
  v
RouterPointService validates frame and builds response
  |
  v
ZigbeeRouterDriver sends PING_RSP unicast
  |
  v
End Device receives response and reads RSSI metadata
```

## 11. Komunikasi Antar Layer

### 11.1 Controller ke Service

Controller memanggil service berdasarkan event:

```cpp
void RouterPointController::handleFrameEvent(const ZigbeeFrame &frame)
{
    router_point_service_.handleIncomingFrame(frame);
}
```

Controller tidak perlu tahu detail payload. Controller hanya tahu bahwa ada frame masuk.

### 11.2 Service ke Driver

Service memakai driver untuk aksi hardware/SDK:

```cpp
zigbee_driver_.sendPingResponse(
    frame.src_short_addr,
    response.bytes,
    response.len
);
```

Service boleh menentukan bahwa response harus dikirim, tetapi pemanggilan SDK tetap terjadi di driver.

### 11.3 Driver ke SDK

Driver menerjemahkan method OOP menjadi API SDK:

```cpp
esp_zb_zcl_custom_cluster_cmd_req(&request);
```

Detail `esp_zb_*` tidak bocor ke service.

## 12. Boundary yang Harus Dijaga

Saat implementasi, jaga batas berikut:

- File `drivers/` boleh include header ESP-IDF dan ESP Zigbee SDK.
- File `services/` tidak boleh include header `esp_zb_*` langsung.
- File `controllers/` boleh include FreeRTOS headers.
- Service tidak boleh membuat task, queue, timer, semaphore, atau event group.
- Driver tidak boleh menjalankan business workflow.
- Callback SDK tidak boleh langsung menjalankan domain logic panjang.
- Semua payload protocol sebaiknya diproses oleh `NavigationProtocolService`.
- Semua konfigurasi router point sebaiknya diakses lewat `RouterConfigService`.
- Semua pengiriman Zigbee response lewat `ZigbeeRouterDriver`.

## 13. Error Handling dan Diagnostic

Error yang perlu dibedakan:

```text
ERR_NOT_NAV_CLUSTER
ERR_NOT_PING_REQ
ERR_PAYLOAD_TOO_SHORT
ERR_SEQUENCE_INVALID
ERR_RESPONSE_DISABLED
ERR_ZIGBEE_NOT_READY
ERR_SEND_RESPONSE_FAILED
```

Counter diagnostic yang berguna:

```text
ping_received_count
ping_accepted_count
response_sent_count
response_failed_count
last_sender_short_addr
last_sequence_id
last_error
```

Counter tersebut sebaiknya berada di controller atau service state, bukan di driver SDK.

## 14. Rencana Implementasi Bertahap

### Tahap 1: Struktur Skeleton

- Ubah `main.c` menjadi `app_main.cpp`.
- Tambahkan folder:
  - `config/`
  - `common/`
  - `drivers/`
  - `services/`
  - `controllers/`
- Update `main/CMakeLists.txt` agar compile semua source C++.
- Tambahkan `sdkconfig.defaults` untuk Router role.

### Tahap 2: Protocol dan Data Types

- Tambahkan `nav_protocol.h`.
- Tambahkan `nav_types.h`.
- Definisikan cluster id `0xFF01`.
- Definisikan command `PING_REQ` dan `PING_RSP`.
- Definisikan payload minimal dan payload dengan `sequence_id`.

### Tahap 3: Zigbee Router Driver

- Implement init stack router.
- Implement custom cluster endpoint.
- Implement decode incoming custom command.
- Implement send unicast response.

### Tahap 4: Service

- Implement `RouterConfigService`.
- Implement `NavigationProtocolService`.
- Implement `RouterPointService::handleIncomingFrame(...)`.

### Tahap 5: Controller FreeRTOS

- Implement `AppController`.
- Implement `ZigbeeController`.
- Implement `RouterPointController`.
- Buat queue frame masuk dari callback ke task.

### Tahap 6: Hardware Validation

- Flash router point ke ESP32-C6.
- Pastikan join sebagai Zigbee Router.
- Pastikan `RxOnWhenIdle = true`.
- Kirim `PING_REQ` dari End Device.
- Validasi Router Point mengirim `PING_RSP` unicast.
- Validasi End Device menerima response dan bisa membaca RSSI metadata.

## 15. Ringkasan Desain

Router Point adalah node statis yang selalu siap menerima ping dan membalas cepat dengan koordinatnya. Desain modularnya adalah:

- `ZigbeeRouterDriver` membungkus ESP Zigbee SDK.
- `StorageDriver` membungkus NVS.
- `StatusLedDriver` membungkus GPIO indikator.
- `RouterConfigService` menyediakan konfigurasi router point.
- `NavigationProtocolService` encode/decode protocol navigation.
- `RouterPointService` memproses ping dan meminta driver mengirim response.
- `ZigbeeController` menangani stack Zigbee dan callback bridge.
- `RouterPointController` menjalankan FreeRTOS event task.
- `AppController` menyusun semua object dan lifecycle.

Dengan struktur ini, firmware tetap modular: driver dan service pasif, sedangkan controller menjadi pusat orchestration FreeRTOS.
