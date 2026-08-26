#pragma once

#include "esp_err.h"

#include "common/nav_types.h"
#include "drivers/storage/storage_driver.hpp"

class RouterConfigService {
public:
    explicit RouterConfigService(StorageDriver &storage_driver);

    esp_err_t init();
    const RouterPointConfig &getConfig() const;
    esp_err_t updateConfig(const RouterPointConfig &config);

private:
    StorageDriver &storage_driver_;
    RouterPointConfig config_ = {};
};
