#include "services/telemetry/rssi_telemetry_service.hpp"

#include <stdio.h>

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
        const uint16_t short_addr = samples[i].router_short_addr;
        int slot = -1;
        int empty_slot = -1;
        for (int j = 0; j < 4; ++j) {
            if (snapshot.routers[j].valid &&
                snapshot.routers[j].short_addr == short_addr) {
                slot = j;
                break;
            }
            if (!snapshot.routers[j].valid && empty_slot < 0) {
                empty_slot = j;
            }
        }
        if (slot < 0) {
            slot = empty_slot;
        }
        if (slot < 0) {
            continue;
        }

        // Keep the strongest duplicate response from the same short address.
        if (!snapshot.routers[slot].valid ||
            samples[i].rssi > snapshot.routers[slot].rssi) {
            snapshot.routers[slot].short_addr = short_addr;
            snapshot.routers[slot].rssi = samples[i].rssi;
            snapshot.routers[slot].valid = true;
        }
    }

    // Stable output for a cycle even when Zigbee responses arrive randomly.
    for (int i = 0; i < 3; ++i) {
        for (int j = i + 1; j < 4; ++j) {
            if (snapshot.routers[j].valid &&
                (!snapshot.routers[i].valid ||
                 snapshot.routers[j].short_addr <
                     snapshot.routers[i].short_addr)) {
                const RssiTelemetryValue temporary = snapshot.routers[i];
                snapshot.routers[i] = snapshot.routers[j];
                snapshot.routers[j] = temporary;
            }
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

    char line[256] = {};
    int length = snprintf(line, sizeof(line), "{\"type\":\"rf\",\"routers\":[");
    bool first = true;
    for (size_t i = 0; i < 4 && length > 0; ++i) {
        if (!snapshot.routers[i].valid) {
            continue;
        }
        const int appended = snprintf(
            line + length,
            sizeof(line) - static_cast<size_t>(length),
            "%s{\"short\":%u,\"rssi\":%d}",
            first ? "" : ",",
            static_cast<unsigned>(snapshot.routers[i].short_addr),
            static_cast<int>(snapshot.routers[i].rssi));
        if (appended <= 0 ||
            appended >= static_cast<int>(sizeof(line) - length)) {
            return ESP_ERR_INVALID_SIZE;
        }
        length += appended;
        first = false;
    }
    const int suffix = snprintf(
        line + length,
        sizeof(line) - static_cast<size_t>(length),
        "]}\n");
    if (suffix <= 0 || suffix >= static_cast<int>(sizeof(line) - length)) {
        return ESP_ERR_INVALID_SIZE;
    }
    length += suffix;
    return driver_.writeLine(line, static_cast<size_t>(length));
}
