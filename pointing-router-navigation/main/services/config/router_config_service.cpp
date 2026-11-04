#include "services/config/router_config_service.hpp"

RouterConfigService::RouterConfigService(StorageDriver &storage_driver)
    : storage_driver_(storage_driver)
{
}

esp_err_t RouterConfigService::init()
{
    return storage_driver_.readRouterConfig(config_);
}

const RouterPointConfig &RouterConfigService::getConfig() const
{
    return config_;
}

esp_err_t RouterConfigService::updateConfig(const RouterPointConfig &config)
{
    esp_err_t err = storage_driver_.writeRouterConfig(config);
    if (err == ESP_OK) {
        config_ = config;
    }
    return err;
}
