# AI SYSTEM CODEX & ARCHITECTURE: ZIGBEE ROUTER (POINT) FOR INDOOR NAVIGATION

## 1. DESKRIPSI SISTEM & PERAN
- **Perangkat**: ESP32-C6
- **Peran Jaringan**: Zigbee Router (ZR) / Pointing Node Statis
- **Topologi**: Terhubung ke Coordinator (Sonoff Zigbee Dongle)
- **Kondisi Daya**: Dicolok langsung ke sumber listrik (Mains-powered). Fitur `RxOnWhenIdle` bernilai TRUE (Radio receiver selalu menyala 100%).
- **Tujuan Utama**: Berfungsi sebagai "Satelit" statis yang mendengarkan sinyal navigasi dari Node yang bergerak, lalu memberikan balasan instan berisi koordinat absolutnya di dalam ruangan.

## 2. ARSITEKTUR KOMUNIKASI (ACTIVE PING / SONAR)
Dalam sistem skala besar ini, Router Point **tidak** melakukan inisiasi pengiriman data secara mandiri. Ia bertindak pasif-reaktif:
1. Router berdiam diri sambil tetap mengaktifkan radio penerima.
2. Saat ada paket **Broadcast (Ping)** dari Node End Device yang lewat, stack Zigbee di Router akan menangkapnya.
3. Router langsung merespons dengan **Unicast** kembali ke alamat pengirim (Node) dengan membawa payload berisikan Koordinat X dan Koordinat Y milik Router tersebut.

## 3. STRUKTUR DIREKTORI PROYEK (ESP-IDF v5.x)
Silakan buat struktur direktori berikut untuk Router Point:
- `router_point/CMakeLists.txt`
- `router_point/main/CMakeLists.txt`
- `router_point/main/main.c` (Entry point, inisialisasi NVS dan stack Zigbee)
- `router_point/main/zb_router_core.c` (File utama berisi Callback Handler dan Custom Cluster 0xFF01)
- `router_point/main/zb_router_core.h` (Header untuk deklarasi variabel statis dan konstanta)
- `router_point/sdkconfig.defaults` (WAJIB berisi: CONFIG_ZB_ZCZR=y)

## 4. SARAN ALGORITMA & LOGIKA KODE (ROUTER)
Karena Router ini berfokus pada kecepatan respons (membalas Ping dari Node), proses penanganan harus dilakukan sesingkat mungkin di dalam *callback* agar tidak memblokir antrean pesan di jaringan mesh.

Berikut adalah referensi logika C yang **harus** diimplementasikan ke dalam file `zb_router_core.c`:

```c
#include "esp_zb_zcl_custom_cluster.h"
#include "zb_router_core.h"

// Konstanta Koordinat Statis Router (Nantinya bisa disimpan di NVS agar dinamis)
#define MY_POS_X 100 // Titik X (contoh: 100 cm)
#define MY_POS_Y 250 // Titik Y (contoh: 250 cm)

#define CUSTOM_NAV_CLUSTER_ID 0xFF01
#define CMD_PING_REQ 0x01
#define CMD_PING_RSP 0x02

// Fungsi Callback Utama Jaringan Zigbee
esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message) {
    if (callback_id == ESP_ZB_CORE_CMD_CUSTOM_CLUSTER_REQ_CB_ID) {
        esp_zb_zcl_custom_cluster_command_message_t *msg = (esp_zb_zcl_custom_cluster_command_message_t *)message;
        
        // Validasi: Apakah ini pesan Broadcast PING_REQ dari Cluster Navigasi?
        if (msg->info.cluster_id == CUSTOM_NAV_CLUSTER_ID && msg->custom_cmd_id == CMD_PING_REQ) {
            
            // Siapkan paket balasan (Unicast ke MAC Address pengirim)
            esp_zb_zcl_custom_cluster_cmd_req_t rsp;
            rsp.zcl_basic_cmd.dst_addr_u.addr_short = msg->info.src_address;
            rsp.cluster_id = CUSTOM_NAV_CLUSTER_ID;
            rsp.custom_cmd_id = CMD_PING_RSP;
            
            // Payload 4 Bytes: [X High, X Low, Y High, Y Low]
            uint8_t payload[4] = {
                (MY_POS_X >> 8) & 0xFF, MY_POS_X & 0xFF,
                (MY_POS_Y >> 8) & 0xFF, MY_POS_Y & 0xFF
            };
            
            rsp.custom_cmd_payload = payload;
            rsp.custom_cmd_payload_length = sizeof(payload);
            
            // Kirim balasan!
            esp_zb_zcl_custom_cluster_cmd_req(&rsp);
        }
    }
    return ESP_OK;
}
```