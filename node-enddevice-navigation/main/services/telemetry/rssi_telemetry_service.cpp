#include "services/telemetry/rssi_telemetry_service.hpp"

#include <stdio.h>

#include "config/navigation_config.h"

RssiTelemetryService::RssiTelemetryService(SerialTelemetryDriver &driver)
    : driver_(driver)
{
}

esp_err_t RssiTelemetryService::init()
{
    const esp_err_t err = driver_.init();
    initialized_ = (err == ESP_OK);
    return err;
}

RssiTelemetrySnapshot RssiTelemetryService::makeSnapshot(
    uint16_t sequence_id,
    const NavAnchorSample *samples,
    size_t sample_count) const
{
    RssiTelemetrySnapshot snapshot = {};
    snapshot.sequence_id = sequence_id;
    if (samples == nullptr) {
        return snapshot;
    }

    for (size_t i = 0; i < sample_count; ++i) {
        const int index = routerIndex(samples[i].router_id);
        if (index < 0) {
            continue;
        }

        // Deterministic duplicate policy: keep the strongest RSSI in a cycle.
        if (!snapshot.routers[index].valid ||
            samples[i].rssi > snapshot.routers[index].rssi) {
            snapshot.routers[index].rssi = samples[i].rssi;
            snapshot.routers[index].valid = true;
        }
    }
    return snapshot;
}

esp_err_t RssiTelemetryService::publish(
    const RssiTelemetrySnapshot &snapshot)
{
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    char values[4][8] = {};
    for (size_t i = 0; i < 4; ++i) {
        if (snapshot.routers[i].valid) {
            snprintf(values[i], sizeof(values[i]), "%d", snapshot.routers[i].rssi);
        } else {
            snprintf(values[i], sizeof(values[i]), "null");
        }
    }

    char line[96] = {};
    const int length = snprintf(
        line,
        sizeof(line),
        "{\"value1\":%s,\"value2\":%s,\"value3\":%s,\"value4\":%s}\n",
        values[0], values[1], values[2], values[3]);
    if (length <= 0 || length >= static_cast<int>(sizeof(line))) {
        return ESP_ERR_INVALID_SIZE;
    }
    return driver_.writeLine(line, static_cast<size_t>(length));
}

int RssiTelemetryService::routerIndex(uint16_t router_id) const
{
    constexpr uint16_t ids[4] = {
        NAVIGATION_ROUTER_R1_ID,
        NAVIGATION_ROUTER_R2_ID,
        NAVIGATION_ROUTER_R3_ID,
        NAVIGATION_ROUTER_R4_ID,
    };

    for (int i = 0; i < 4; ++i) {
        if (ids[i] != 0 && router_id == ids[i]) {
            return i;
        }
    }
    return -1;
}
