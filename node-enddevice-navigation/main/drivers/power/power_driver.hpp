#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_sleep.h"

class PowerDriver {
public:
    esp_err_t init();
    esp_sleep_wakeup_cause_t getWakeupCause() const;
    esp_err_t enterLightSleep(uint32_t duration_ms);
    esp_err_t enterDeepSleep(uint32_t duration_ms);
};

