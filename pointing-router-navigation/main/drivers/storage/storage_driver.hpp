#pragma once

#include "esp_err.h"

#include "common/nav_types.h"

class StorageDriver {
public:
    esp_err_t init();
    esp_err_t readRouterConfig(RouterPointConfig &out_config);
    esp_err_t writeRouterConfig(const RouterPointConfig &config);
};
