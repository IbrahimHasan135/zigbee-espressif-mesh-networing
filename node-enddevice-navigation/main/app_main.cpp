#include "controllers/app_controller.hpp"

#include "esp_err.h"

extern "C" void app_main(void)
{
    static AppController app_controller;

    esp_err_t err = app_controller.init();
    if (err != ESP_OK) {
        return;
    }

    err = app_controller.start();
    if (err != ESP_OK) {
        return;
    }
}
