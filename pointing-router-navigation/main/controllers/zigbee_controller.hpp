#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "ezbee/app_signals.h"
#include "ezbee/zcl/cluster/custom.h"

#include "controllers/router_point_controller.hpp"
#include "drivers/zigbee/zigbee_router_driver.hpp"

class ZigbeeController {
public:
    ZigbeeController(ZigbeeRouterDriver &zigbee_driver,
                     RouterPointController &router_point_controller);

    esp_err_t start();
    bool handleAppSignal(const ezb_app_signal_t *signal);
    ezb_zcl_status_t handleCustomClusterCommand(const ezb_zcl_cmd_hdr_t *header,
                                                const uint8_t *payload,
                                                uint16_t payload_length);

private:
    static void taskEntry(void *arg);
    static bool appSignalHandler(const ezb_app_signal_t *signal);
    static ezb_zcl_status_t customClusterCommandHandler(const ezb_zcl_cmd_hdr_t *header,
                                                        const uint8_t *payload,
                                                        uint16_t payload_length);
    static ZigbeeController *instance();

    void task();
    const char *signalName(uint32_t signal) const;

    ZigbeeRouterDriver &zigbee_driver_;
    RouterPointController &router_point_controller_;
    static ZigbeeController *instance_;
};
