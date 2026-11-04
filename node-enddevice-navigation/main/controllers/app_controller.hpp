#pragma once

#include "controllers/navigation_controller.hpp"
#include "controllers/zigbee_controller.hpp"
#include "drivers/power/power_driver.hpp"
#include "drivers/storage/storage_driver.hpp"
#include "drivers/zigbee/zigbee_end_device_driver.hpp"
#include "services/navigation/navigation_service.hpp"
#include "services/positioning/positioning_service.hpp"
#include "services/power/power_service.hpp"

class AppController {
public:
    AppController();

    esp_err_t init();
    esp_err_t start();

private:
    StorageDriver storage_driver_;
    PowerDriver power_driver_;
    ZigbeeEndDeviceDriver zigbee_driver_;

    NavigationService navigation_service_;
    PositioningService positioning_service_;
    PowerService power_service_;

    NavigationController navigation_controller_;
    ZigbeeController zigbee_controller_;
};

