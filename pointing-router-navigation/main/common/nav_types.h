#pragma once

#include <stdint.h>

#include "esp_err.h"

#include "common/app_error.h"
#include "common/nav_protocol.h"

struct ZigbeeFrame {
    uint16_t src_short_addr = 0;
    uint16_t dst_short_addr = 0;
    uint8_t src_endpoint = 0;
    uint8_t dst_endpoint = 0;
    uint16_t cluster_id = 0;
    uint8_t command_id = 0;
    int8_t rssi = 0;
    uint8_t lqi = 0;
    uint8_t payload[32] = {};
    uint8_t payload_len = 0;
};

struct RouterPointConfig {
    uint16_t router_id = 0;
    uint16_t x_cm = 0;
    uint16_t y_cm = 0;
    bool response_enabled = true;
};

struct PingRequest {
    uint16_t sequence_id = 0;
    uint16_t sender_short_addr = 0;
    bool has_sequence_id = false;
};

struct PingResponsePayload {
    uint8_t bytes[PING_RSP_FULL_LEN] = {};
    uint8_t len = 0;
};

struct RouterPointProcessResult {
    AppStatus status = AppStatus::kOk;
    esp_err_t driver_error = ESP_OK;
    uint16_t sender_short_addr = 0;
    uint16_t sequence_id = 0;
    bool has_sequence_id = false;
};
