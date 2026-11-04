#pragma once

#include "esp_err.h"

#include "common/nav_types.h"

class StorageDriver {
public:
    esp_err_t init();
    esp_err_t readNodeConfig(NodeConfig &out_config);
    esp_err_t writeNodeConfig(const NodeConfig &config);
};

