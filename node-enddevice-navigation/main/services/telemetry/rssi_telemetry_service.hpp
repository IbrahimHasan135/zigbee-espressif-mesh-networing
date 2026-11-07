#pragma once

#include <stddef.h>

#include "common/nav_types.h"
#include "common/rssi_telemetry_types.h"
#include "drivers/serial/serial_telemetry_driver.hpp"

class RssiTelemetryService {
public:
    explicit RssiTelemetryService(SerialTelemetryDriver &driver);

    esp_err_t init();
    RssiTelemetrySnapshot makeSnapshot(
        uint16_t sequence_id,
        const NavAnchorSample *samples,
        size_t sample_count) const;
    esp_err_t publish(const RssiTelemetrySnapshot &snapshot);

private:
    SerialTelemetryDriver &driver_;
    bool initialized_ = false;
};
