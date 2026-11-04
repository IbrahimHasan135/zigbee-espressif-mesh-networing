#include "drivers/storage/storage_driver.hpp"

#include "nvs.h"
#include "nvs_flash.h"

#include "config/router_point_config.h"

namespace {

constexpr char kNamespace[] = "router_point";
constexpr char kRouterIdKey[] = "router_id";
constexpr char kPosXKey[] = "x_cm";
constexpr char kPosYKey[] = "y_cm";
constexpr char kResponseEnabledKey[] = "rsp_en";

RouterPointConfig defaultConfig()
{
    RouterPointConfig config = {};
    config.router_id = ROUTER_POINT_DEFAULT_ID;
    config.x_cm = ROUTER_POINT_DEFAULT_X_CM;
    config.y_cm = ROUTER_POINT_DEFAULT_Y_CM;
    config.response_enabled = ROUTER_POINT_DEFAULT_RESPONSE_ENABLED;
    return config;
}

} // namespace

esp_err_t StorageDriver::init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            return err;
        }
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t StorageDriver::readRouterConfig(RouterPointConfig &out_config)
{
    out_config = defaultConfig();

    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    uint16_t router_id = out_config.router_id;
    uint16_t x_cm = out_config.x_cm;
    uint16_t y_cm = out_config.y_cm;
    uint8_t response_enabled = out_config.response_enabled ? 1 : 0;

    if (nvs_get_u16(handle, kRouterIdKey, &router_id) == ESP_OK) {
        out_config.router_id = router_id;
    }
    if (nvs_get_u16(handle, kPosXKey, &x_cm) == ESP_OK) {
        out_config.x_cm = x_cm;
    }
    if (nvs_get_u16(handle, kPosYKey, &y_cm) == ESP_OK) {
        out_config.y_cm = y_cm;
    }
    if (nvs_get_u8(handle, kResponseEnabledKey, &response_enabled) == ESP_OK) {
        out_config.response_enabled = response_enabled != 0;
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t StorageDriver::writeRouterConfig(const RouterPointConfig &config)
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u16(handle, kRouterIdKey, config.router_id);
    if (err == ESP_OK) {
        err = nvs_set_u16(handle, kPosXKey, config.x_cm);
    }
    if (err == ESP_OK) {
        err = nvs_set_u16(handle, kPosYKey, config.y_cm);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, kResponseEnabledKey, config.response_enabled ? 1 : 0);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}
