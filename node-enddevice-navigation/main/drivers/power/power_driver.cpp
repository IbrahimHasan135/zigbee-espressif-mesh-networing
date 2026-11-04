#include "drivers/power/power_driver.hpp"

esp_err_t PowerDriver::init()
{
    return ESP_OK;
}

esp_sleep_wakeup_cause_t PowerDriver::getWakeupCause() const
{
    return esp_sleep_get_wakeup_cause();
}

esp_err_t PowerDriver::enterLightSleep(uint32_t duration_ms)
{
    esp_err_t err = esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(duration_ms) * 1000ULL);
    if (err != ESP_OK) {
        return err;
    }
    return esp_light_sleep_start();
}

esp_err_t PowerDriver::enterDeepSleep(uint32_t duration_ms)
{
    esp_err_t err = esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(duration_ms) * 1000ULL);
    if (err != ESP_OK) {
        return err;
    }
    esp_deep_sleep_start();
    return ESP_OK;
}
