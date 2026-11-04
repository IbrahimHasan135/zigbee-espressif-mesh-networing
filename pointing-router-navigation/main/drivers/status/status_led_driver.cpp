#include "drivers/status/status_led_driver.hpp"

#include "driver/gpio.h"

#include "config/router_point_config.h"

esp_err_t StatusLedDriver::init()
{
#if STATUS_LED_GPIO >= 0
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << STATUS_LED_GPIO;
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;

    esp_err_t err = gpio_config(&config);
    if (err == ESP_OK) {
        initialized_ = true;
    }
    return err;
#else
    initialized_ = false;
    return ESP_OK;
#endif
}

void StatusLedDriver::setBooting()
{
    if (initialized_) {
        gpio_set_level(static_cast<gpio_num_t>(STATUS_LED_GPIO), 1);
    }
}

void StatusLedDriver::setJoined()
{
    if (initialized_) {
        gpio_set_level(static_cast<gpio_num_t>(STATUS_LED_GPIO), 1);
    }
}

void StatusLedDriver::pulsePingReceived()
{
    if (initialized_) {
        gpio_set_level(static_cast<gpio_num_t>(STATUS_LED_GPIO), 0);
        gpio_set_level(static_cast<gpio_num_t>(STATUS_LED_GPIO), 1);
    }
}

void StatusLedDriver::pulseResponseSent()
{
    if (initialized_) {
        gpio_set_level(static_cast<gpio_num_t>(STATUS_LED_GPIO), 1);
    }
}
