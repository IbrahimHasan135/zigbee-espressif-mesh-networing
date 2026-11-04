#pragma once

#include "esp_err.h"

#include "controllers/router_point_controller.hpp"
#include "controllers/zigbee_controller.hpp"
#include "drivers/status/status_led_driver.hpp"
#include "drivers/storage/storage_driver.hpp"
#include "drivers/zigbee/zigbee_router_driver.hpp"
#include "services/config/router_config_service.hpp"
#include "services/navigation_protocol/navigation_protocol_service.hpp"
#include "services/router_point/router_point_service.hpp"

class AppController {
public:
    AppController();

    esp_err_t init();
    esp_err_t start();

private:
    StorageDriver storage_driver_;
    StatusLedDriver status_led_driver_;
    ZigbeeRouterDriver zigbee_driver_;

    RouterConfigService config_service_;
    NavigationProtocolService protocol_service_;
    RouterPointService router_point_service_;

    RouterPointController router_point_controller_;
    ZigbeeController zigbee_controller_;
};
