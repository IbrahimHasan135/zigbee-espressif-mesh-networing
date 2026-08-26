#pragma once

#include "esp_err.h"
#include "esp_zigbee.h"
#include "ezbee/app_signals.h"
#include "ezbee/zcl/cluster/custom.h"

#include "common/nav_types.h"

class ZigbeeEndDeviceDriver {
public:
    esp_err_t init();
    esp_err_t registerDevice();
    esp_err_t registerAppSignalHandler(ezb_app_signal_handler_t handler);
    esp_err_t registerCustomClusterHandler(ezb_zcl_custom_cluster_process_cmd_t handler);
    esp_err_t start(bool autostart);
    void runMainLoop();

    esp_err_t startCommissioning(ezb_bdb_comm_mode_mask_t mode_mask);
    esp_err_t configureSleep(bool enable, uint32_t threshold_ms);
    esp_err_t setRxOnWhenIdle(bool enabled);
    esp_err_t sendBroadcastPing(uint16_t sequence_id);

    bool decodeCustomCommand(const ezb_zcl_cmd_hdr_t *header,
                             const uint8_t *payload,
                             uint16_t payload_length,
                             ZigbeeFrame &out_frame) const;

    void setNetworkReady(bool ready);
    bool isNetworkReady() const;

private:
    bool initialized_ = false;
    bool network_ready_ = false;
};
