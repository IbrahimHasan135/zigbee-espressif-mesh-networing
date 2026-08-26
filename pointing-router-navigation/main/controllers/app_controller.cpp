#include "controllers/app_controller.hpp"

#include "esp_log.h"

namespace {

constexpr char kTag[] = "AppController";

} // namespace

AppController::AppController()
    : config_service_(storage_driver_),
      router_point_service_(zigbee_driver_, config_service_, protocol_service_),
      router_point_controller_(router_point_service_, status_led_driver_),
      zigbee_controller_(zigbee_driver_, router_point_controller_)
{
}

esp_err_t AppController::init()
{
    ESP_LOGI(kTag, "initializing storage driver");
    esp_err_t err = storage_driver_.init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "storage init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "initializing status LED driver");
    err = status_led_driver_.init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "status LED init failed: %s", esp_err_to_name(err));
        return err;
    }
    status_led_driver_.setBooting();

    ESP_LOGI(kTag, "loading router point config");
    err = config_service_.init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "router config service init failed: %s", esp_err_to_name(err));
        return err;
    }

    const RouterPointConfig &config = config_service_.getConfig();
    ESP_LOGI(
        kTag,
        "router config loaded, id=0x%04x x=%ucm y=%ucm response=%s",
        config.router_id,
        config.x_cm,
        config.y_cm,
        config.response_enabled ? "enabled" : "disabled"
    );

    ESP_LOGI(kTag, "initializing router point controller");
    err = router_point_controller_.init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "router point controller init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "app init complete");
    return ESP_OK;
}

esp_err_t AppController::start()
{
    ESP_LOGI(kTag, "starting Zigbee controller");
    esp_err_t err = zigbee_controller_.start();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Zigbee controller start failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "starting router point controller");
    err = router_point_controller_.start();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "router point controller start failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "app started");
    return ESP_OK;
}
