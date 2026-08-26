#include "controllers/app_controller.hpp"

extern "C" void app_main(void)
{
    static AppController app;
    esp_err_t err = app.init();
    if (err != ESP_OK) {
        return;
    }
    (void)app.start();
}
