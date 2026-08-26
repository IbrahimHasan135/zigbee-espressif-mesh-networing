#include "drivers/storage/storage_driver.hpp"

#include "nvs.h"
#include "nvs_flash.h"

#include "config/navigation_config.h"

namespace {

constexpr char kNamespace[] = "nav_node";
constexpr char kNodeIdKey[] = "node_id";
constexpr char kListenWindowKey[] = "listen_ms";
constexpr char kSleepIntervalKey[] = "sleep_ms";
constexpr char kRetryIntervalKey[] = "retry_ms";

NodeConfig defaultConfig()
{
    NodeConfig config = {};
    config.node_id = kDefaultNodeId;
    config.listen_window_ms = NAVIGATION_LISTEN_WINDOW_MS;
    config.sleep_interval_ms = NAVIGATION_SLEEP_INTERVAL_MS;
    config.retry_interval_ms = NAVIGATION_RETRY_INTERVAL_MS;
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

esp_err_t StorageDriver::readNodeConfig(NodeConfig &out_config)
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

    uint16_t node_id = out_config.node_id;
    uint32_t listen_window_ms = out_config.listen_window_ms;
    uint32_t sleep_interval_ms = out_config.sleep_interval_ms;
    uint32_t retry_interval_ms = out_config.retry_interval_ms;

    if (nvs_get_u16(handle, kNodeIdKey, &node_id) == ESP_OK) {
        out_config.node_id = node_id;
    }
    if (nvs_get_u32(handle, kListenWindowKey, &listen_window_ms) == ESP_OK) {
        out_config.listen_window_ms = listen_window_ms;
    }
    if (nvs_get_u32(handle, kSleepIntervalKey, &sleep_interval_ms) == ESP_OK) {
        out_config.sleep_interval_ms = sleep_interval_ms;
    }
    if (nvs_get_u32(handle, kRetryIntervalKey, &retry_interval_ms) == ESP_OK) {
        out_config.retry_interval_ms = retry_interval_ms;
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t StorageDriver::writeNodeConfig(const NodeConfig &config)
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u16(handle, kNodeIdKey, config.node_id);
    if (err == ESP_OK) {
        err = nvs_set_u32(handle, kListenWindowKey, config.listen_window_ms);
    }
    if (err == ESP_OK) {
        err = nvs_set_u32(handle, kSleepIntervalKey, config.sleep_interval_ms);
    }
    if (err == ESP_OK) {
        err = nvs_set_u32(handle, kRetryIntervalKey, config.retry_interval_ms);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}
