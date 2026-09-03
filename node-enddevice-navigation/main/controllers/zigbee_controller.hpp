#pragma once

#include "esp_err.h"
#include "esp_zigbee.h"
#include "ezbee/app_signals.h"
#include "ezbee/zcl/cluster/custom.h"

#include "controllers/navigation_controller.hpp"
#include "drivers/zigbee/zigbee_end_device_driver.hpp"

class ZigbeeController {
public:
    ZigbeeController(ZigbeeEndDeviceDriver &zigbee_driver,
                     NavigationController &navigation_controller);

    esp_err_t start();
    bool handleAppSignal(const ezb_app_signal_t *signal);
    ezb_zcl_status_t handleCustomClusterCommand(const ezb_zcl_cmd_hdr_t *header,
                                                const uint8_t *payload,
                                                uint16_t payload_length);

    static ZigbeeController *instance();

private:
    static void taskEntry(void *arg);
    static void commissioningRetryTaskEntry(void *arg);
    static bool appSignalHandler(const ezb_app_signal_t *signal);
    static ezb_zcl_status_t customClusterCommandHandler(const ezb_zcl_cmd_hdr_t *header,
                                                        const uint8_t *payload,
                                                        uint16_t payload_length);

    void task();
    void commissioningRetryTask();
    void scheduleCommissioningRetry(ezb_bdb_comm_mode_mask_t mode_mask);
    const char *commissioningModeName(ezb_bdb_comm_mode_mask_t mode_mask) const;
    const char *signalName(uint32_t signal) const;

    ZigbeeEndDeviceDriver &zigbee_driver_;
    NavigationController &navigation_controller_;
    ezb_bdb_comm_mode_mask_t retry_commissioning_mode_ = 0;
    bool commissioning_retry_task_running_ = false;
    static ZigbeeController *instance_;
};
