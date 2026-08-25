# AI SYSTEM CODEX & ARCHITECTURE: ZIGBEE END DEVICE (NODE) FOR INDOOR NAVIGATION

## 1. DESKRIPSI SISTEM & PERAN
- **Perangkat**: ESP32-C6
- **Peran Jaringan**: Sleepy End Device (SED) / ZED
- **Topologi**: Terhubung ke Coordinator (Sonoff Zigbee Dongle)
- **Kondisi Daya**: Menggunakan Baterai. Membutuhkan efisiensi tingkat tinggi melalui fitur Light/Deep Sleep.
- **Tujuan Utama**: Berfungsi sebagai "Penerima GPS". Ia mengirim sinyal ke sekelilingnya, menangkap balasan dari titik-titik statis (Router), mengekstrak RSSI, dan menghitung lokasinya sendiri.

## 2. ANALISIS ARSITEKTUR: FOKUS EFISIENSI ENERGI (ACTIVE PING / SONAR)
Untuk menghemat energi secara drastis, Node harus di-set sebagai Sleepy End Device (SED). Karena SED mematikan radionya saat tidur, ia menggunakan metode "Sonar Aktif":
1. **Tidur Panjang:** Node (SED) berada dalam mode Deep Sleep (konsumsi daya micro-Ampere).
2. **Bangun & Ping:** Node bangun secara periodik, menyalakan radio, dan mengirimkan paket Broadcast ringan ("Saya di sini").
3. **Mendengarkan Sesaat:** Node tetap membuka radionya (Rx ON) selama ~150 ms untuk menunggu balasan.
4. **Respon Points:** Point (Router statis) membalas secara Unicast ke Node. Balasan berisi `[ID_Point, Koordinat_X, Koordinat_Y]`.
5. **Kalkulasi & Tidur:** Node mengumpulkan balasan, membaca RSSI dari masing-masing balasan, menghitung lokasinya, lalu kembali tidur.

## 3. ANALISIS BEBAN PEMROSESAN RSSI VIA CALLBACK
Menangkap nilai RSSI dengan membaca struct data pada level Network/MAC layer callback tidak akan memberatkan prosesor ESP32-C6 sama sekali.
- Proses di dalam callback hanyalah operasi membaca pointer di memori dan memasukkannya ke dalam *queue* lokal (memakan waktu beberapa *microsecond*).
- Praktik Terbaik: Selesaikan fungsi callback secepat mungkin dan lempar data ke FreeRTOS `xQueueSend`. Buat *Task* FreeRTOS terpisah untuk mengambil data dari *Queue* dan mengeksekusi komputasi berat (Trilateration).

## 4. STRUKTUR DIREKTORI PROYEK (ESP-IDF v5.x)
Silakan buat struktur direktori berikut untuk Node End Device:
- `node_end_device/CMakeLists.txt`
- `node_end_device/main/CMakeLists.txt`
- `node_end_device/main/main.c` (Inisialisasi antrean Queue dan Task Navigasi)
- `node_end_device/main/zb_node_core.c` (Implementasi Active Ping, Timer Sleep, dan Callback RSSI)
- `node_end_device/main/zb_node_core.h`
- `node_end_device/main/trilateration.c` (Berisi algoritma matematika *floating-point*)
- `node_end_device/sdkconfig.defaults` (WAJIB berisi: CONFIG_ZB_ZED=y)

## 5. SARAN ALGORITMA & LOGIKA KODE (NODE END DEVICE)
Kode ini mengimplementasikan pemisahan tugas (*decoupling*) antara proses Jaringan (via Callback) dan proses Perhitungan (via FreeRTOS Task).

Berikut adalah referensi logika C yang **harus** diimplementasikan ke dalam file `zb_node_core.c`:

```c
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_zb_zcl_custom_cluster.h"
#include "zb_node_core.h"

#define CUSTOM_NAV_CLUSTER_ID 0xFF01
#define CMD_PING_REQ 0x01
#define CMD_PING_RSP 0x02

// Struktur data mentah yang akan dilempar ke Queue
typedef struct {
    uint16_t router_mac;
    uint16_t x;
    uint16_t y;
    int8_t rssi;
} nav_data_t;

// Queue terhubung dengan main.c
QueueHandle_t rssi_data_queue; 

/* =========================================================
   TASK 1: PING LIFECYCLE (Berjalan Terpisah dari Zigbee)
   ========================================================= */
void nav_controller_task(void *pvParameters) {
    while(1) {
        // 1. Bangun dari tidur & Kirim PING (Broadcast)
        esp_zb_zcl_custom_cluster_cmd_req_t req;
        req.zcl_basic_cmd.dst_addr_u.addr_short = 0xFFFD; // RxOnWhenIdle devices
        req.cluster_id = CUSTOM_NAV_CLUSTER_ID;
        req.custom_cmd_id = CMD_PING_REQ;
        req.custom_cmd_payload = NULL;
        req.custom_cmd_payload_length = 0;
        
        esp_zb_zcl_custom_cluster_cmd_req(&req);

        // 2. Jendela Mendengar (Listening Window)
        vTaskDelay(pdMS_TO_TICKS(150)); 
        
        // 3. (Proses Data Queue untuk Trilateration dieksekusi di sini)
        
        // 4. Tidur Panjang untuk Menghemat Baterai
        vTaskDelay(pdMS_TO_TICKS(5000)); // Tidur 5 Detik
    }
}

/* =========================================================
   TASK 2 / CALLBACK: TANGKAP RSSI SUPER CEPAT (O(1))
   ========================================================= */
esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message) {
    if (callback_id == ESP_ZB_CORE_CMD_CUSTOM_CLUSTER_REQ_CB_ID) {
        esp_zb_zcl_custom_cluster_command_message_t *msg = (esp_zb_zcl_custom_cluster_command_message_t *)message;
        
        // Validasi: Apakah ini balasan dari Router statis?
        if (msg->info.cluster_id == CUSTOM_NAV_CLUSTER_ID && msg->custom_cmd_id == CMD_PING_RSP) {
            
            // Ekstrak Payload Koordinat
            uint16_t router_x = (msg->data.value[0] << 8) | msg->data.value[1];
            uint16_t router_y = (msg->data.value[2] << 8) | msg->data.value[3];
            
            // Ekstrak Nilai LQI / RSSI dari metadata Network Layer
            int8_t packet_rssi = msg->info.rssi; 

            // Kemas ke struct
            nav_data_t new_data = {
                .router_mac = msg->info.src_address,
                .x = router_x,
                .y = router_y,
                .rssi = packet_rssi
            };
            
            // Lempar ke Antrean FreeRTOS seketika tanpa komputasi berat
            xQueueSend(rssi_data_queue, &new_data, 0);
        }
    }
    return ESP_OK;
}
```