#pragma once

#include <stdint.h>

struct RssiTelemetryValue {
    int8_t rssi = 0;
    bool valid = false;
};

struct RssiTelemetrySnapshot {
    uint16_t sequence_id = 0;
    RssiTelemetryValue routers[4] = {};
};
