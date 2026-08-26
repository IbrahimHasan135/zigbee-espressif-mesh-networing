#include "controllers/app_controller.hpp"

#include "esp_err.h"
#include "esp_log.h"

namespace {

constexpr char kTag[] = "AppController";

} // namespace

AppController::AppController()
    : navigation_service_(zigbee_driver_),
      power_service_(power_driver_, storage_driver_),
      navigation_controller_(navigation_service_, positioning_service_, power_service_),
      zigbee_controller_(zigbee_driver_, navigation_controller_)
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

    ESP_LOGI(kTag, "initializing power driver");
    err = power_driver_.init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "power init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "loading power/navigation service config");
    err = power_service_.init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "power service init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "initializing navigation controller");
    err = navigation_controller_.init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "navigation controller init failed: %s", esp_err_to_name(err));
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

    ESP_LOGI(kTag, "starting navigation controller");
    err = navigation_controller_.start();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "navigation controller start failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "app started");
    return ESP_OK;
}

