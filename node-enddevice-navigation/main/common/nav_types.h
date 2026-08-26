#pragma once

#include <stddef.h>
#include <stdint.h>

#include "common/app_error.h"

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

struct NavAnchorSample {
    uint16_t sequence_id = 0;
    uint16_t router_id = 0;
    uint16_t router_short_addr = 0;
    float x_cm = 0.0f;
    float y_cm = 0.0f;
    int8_t rssi = 0;
    uint8_t lqi = 0;
    uint32_t received_at_ms = 0;
};

struct Position2D {
    float x_cm = 0.0f;
    float y_cm = 0.0f;
    float confidence = 0.0f;
};

struct NodeConfig {
    uint16_t node_id = 0;
    uint32_t listen_window_ms = 0;
    uint32_t sleep_interval_ms = 0;
    uint32_t retry_interval_ms = 0;
};

struct NavigationCycleResult {
    AppStatus status = AppStatus::kOk;
    uint16_t sequence_id = 0;
    size_t sample_count = 0;
    Position2D position = {};
};

