#pragma once

#include <stddef.h>
#include <stdint.h>

#include "common/nav_types.h"
#include "config/navigation_config.h"
#include "drivers/zigbee/zigbee_end_device_driver.hpp"

class NavigationService {
public:
    explicit NavigationService(ZigbeeEndDeviceDriver &zigbee_driver);

    AppStatus beginCycle(uint16_t sequence_id);
    esp_err_t sendPing();
    AppStatus acceptFrame(const ZigbeeFrame &frame, uint32_t received_at_ms);
    NavigationCycleResult finishCycle(const Position2D *position);

    uint16_t activeSequenceId() const;
    size_t sampleCount() const;
    const NavAnchorSample *samples() const;

private:
    AppStatus decodeResponse(const ZigbeeFrame &frame, NavAnchorSample &out_sample) const;

    ZigbeeEndDeviceDriver &zigbee_driver_;
    uint16_t active_sequence_id_ = 0;
    NavAnchorSample samples_[NAVIGATION_MAX_SAMPLES] = {};
    size_t sample_count_ = 0;
    bool cycle_active_ = false;
};
