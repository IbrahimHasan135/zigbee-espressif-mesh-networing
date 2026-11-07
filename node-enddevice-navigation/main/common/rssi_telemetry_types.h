#pragma once

#include <stdint.h>

struct RssiTelemetryValue {
    uint16_t short_addr = 0;
    int8_t rssi = 0;
    bool valid = false;
};

struct RssiTelemetrySnapshot {
    uint16_t sequence_id = 0;
    RssiTelemetryValue routers[4] = {};
};
