#include "drivers/zigbee/zigbee_router_driver.hpp"

#include <algorithm>
#include <cstring>

#include "ezbee/af.h"
#include "ezbee/bdb.h"
#include "ezbee/core.h"
#include "ezbee/nwk.h"
#include "ezbee/zcl/cluster/custom.h"
#include "ezbee/zcl/zcl_type.h"
#include "ezbee/zha.h"

#include "common/nav_protocol.h"
#include "config/zigbee_config.h"

namespace {

esp_zigbee_config_t makeZigbeeConfig()
{
    esp_zigbee_config_t config = {};
    config.device_config.device_type = EZB_NWK_DEVICE_TYPE_ROUTER;
    config.device_config.install_code_policy = false;
    config.device_config.zczr_config.max_children = ZIGBEE_ROUTER_MAX_CHILDREN;
    config.platform_config.storage_partition_name = nullptr;
    config.platform_config.radio_config.radio_mode = ESP_ZIGBEE_RADIO_MODE_NATIVE;
    return config;
}

} // namespace

esp_err_t ZigbeeRouterDriver::init()
{
    esp_zigbee_config_t zigbee_config = makeZigbeeConfig();
    esp_err_t err = esp_zigbee_init(&zigbee_config);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_zigbee_err_to_esp(ezb_bdb_set_primary_channel_set(ZIGBEE_PRIMARY_CHANNEL_MASK));
    if (err != ESP_OK) {
        return err;
    }

    err = esp_zigbee_err_to_esp(ezb_bdb_set_secondary_channel_set(ZIGBEE_SECONDARY_CHANNEL_MASK));
    if (err != ESP_OK) {
        return err;
    }

    ezb_bdb_set_scan_duration(ZIGBEE_BDB_SCAN_DURATION);
    ezb_bdb_set_router_rejoin_required(true);

    initialized_ = true;
    return ESP_OK;
}

esp_err_t ZigbeeRouterDriver::registerDevice()
{
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    ezb_af_device_desc_t device_desc = ezb_af_create_device_desc();
    if (device_desc == EZB_INVALID_AF_DEVICE_DESC) {
        return ESP_ERR_NO_MEM;
    }

    ezb_zha_custom_gateway_config_t gateway_config = EZB_ZHA_CUSTOM_GATEWAY_CONFIG();
    gateway_config.basic_cfg.power_source = EZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE;
    ezb_af_ep_desc_t endpoint_desc =
        ezb_zha_create_custom_gateway(ZIGBEE_ENDPOINT_NAVIGATION, &gateway_config);
    if (endpoint_desc == EZB_INVALID_AF_EP_DESC) {
        return ESP_ERR_NO_MEM;
    }

    ezb_zcl_custom_cluster_config_t navigation_cluster_config = {};
    navigation_cluster_config.cluster_id = CUSTOM_NAV_CLUSTER_ID;
    navigation_cluster_config.init_func = nullptr;
    navigation_cluster_config.deinit_func = nullptr;

    ezb_zcl_cluster_desc_t navigation_server_cluster = ezb_zcl_custom_create_cluster_desc(
        &navigation_cluster_config,
        EZB_ZCL_CLUSTER_SERVER
    );
    if (navigation_server_cluster == EZB_INVALID_ZCL_CLUSTER_DESC) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_zigbee_err_to_esp(
        ezb_af_endpoint_add_cluster_desc(endpoint_desc, navigation_server_cluster)
    );
    if (err != ESP_OK) {
        return err;
    }

    err = esp_zigbee_err_to_esp(ezb_af_device_add_endpoint_desc(device_desc, endpoint_desc));
    if (err != ESP_OK) {
        return err;
    }
    return esp_zigbee_err_to_esp(ezb_af_device_desc_register(device_desc));
}

esp_err_t ZigbeeRouterDriver::registerAppSignalHandler(ezb_app_signal_handler_t handler)
{
    if (handler == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_zigbee_err_to_esp(ezb_app_signal_add_handler(handler));
}

esp_err_t ZigbeeRouterDriver::registerCustomClusterHandler(ezb_zcl_custom_cluster_process_cmd_t handler)
{
    if (handler == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    ezb_zcl_custom_cluster_handlers_t handlers = {};
    handlers.cluster_id = CUSTOM_NAV_CLUSTER_ID;
    handlers.cluster_role = EZB_ZCL_CLUSTER_SERVER;
    handlers.process_cmd_cb = handler;

    return esp_zigbee_err_to_esp(ezb_zcl_custom_cluster_handlers_register(&handlers));
}

esp_err_t ZigbeeRouterDriver::start(bool autostart)
{
    return esp_zigbee_start(autostart);
}

void ZigbeeRouterDriver::runMainLoop()
{
    (void)esp_zigbee_launch_mainloop();
}

esp_err_t ZigbeeRouterDriver::startCommissioning(ezb_bdb_comm_mode_mask_t mode_mask)
{
    return esp_zigbee_err_to_esp(ezb_bdb_start_top_level_commissioning(mode_mask));
}

esp_err_t ZigbeeRouterDriver::postCommissioning(ezb_bdb_comm_mode_mask_t mode_mask)
{
    pending_commissioning_mode_ = mode_mask;
    return esp_zigbee_task_queue_post(commissioningTaskCallback, this);
}

void ZigbeeRouterDriver::commissioningTaskCallback(void *ctx)
{
    ZigbeeRouterDriver *driver = static_cast<ZigbeeRouterDriver *>(ctx);
    if (driver == nullptr) {
        return;
    }
    (void)ezb_bdb_start_top_level_commissioning(driver->pending_commissioning_mode_);
}

esp_err_t ZigbeeRouterDriver::setRxOnWhenIdle(bool enabled)
{
    ezb_set_rx_on_when_idle(enabled);
    return ESP_OK;
}

esp_err_t ZigbeeRouterDriver::sendPingResponse(uint16_t dst_short_addr,
                                               const uint8_t *payload,
                                               uint8_t payload_len)
{
    if (!network_ready_) {
        return ESP_ERR_INVALID_STATE;
    }
    if (payload == nullptr || payload_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ezb_zcl_custom_cluster_cmd_t response = {};
    response.cmd_ctrl.dst_addr.addr_mode = EZB_ADDR_MODE_SHORT;
    response.cmd_ctrl.dst_addr.u.short_addr = dst_short_addr;
    response.cmd_ctrl.dst_ep = ZIGBEE_ENDPOINT_NAVIGATION;
    response.cmd_ctrl.src_ep = ZIGBEE_ENDPOINT_NAVIGATION;
    response.cmd_ctrl.cluster_id = CUSTOM_NAV_CLUSTER_ID;
    response.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
    response.cmd_ctrl.fc.manuf_specific = EZB_ZCL_NOT_MANUFACTURER_SPECIFIC;
    response.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
    response.cmd_ctrl.fc.dis_default_rsp = 1;
    response.cmd_id = CMD_PING_RSP;
    response.data_length = payload_len;
    response.data = const_cast<uint8_t *>(payload);

    if (!esp_zigbee_lock_acquire(0)) {
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = esp_zigbee_err_to_esp(ezb_zcl_custom_cluster_cmd_req(&response));
    esp_zigbee_lock_release();

    return err;
}

bool ZigbeeRouterDriver::decodeCustomCommand(const ezb_zcl_cmd_hdr_t *header,
                                             const uint8_t *payload,
                                             uint16_t payload_length,
                                             ZigbeeFrame &out_frame) const
{
    if (header == nullptr) {
        return false;
    }

    out_frame = {};
    out_frame.src_short_addr = header->src_addr.u.short_addr;
    out_frame.dst_short_addr = header->dst_addr.u.short_addr;
    out_frame.src_endpoint = header->src_ep;
    out_frame.dst_endpoint = header->dst_ep;
    out_frame.cluster_id = header->cluster_id;
    out_frame.command_id = header->cmd_id;
    out_frame.rssi = header->rssi;
    out_frame.lqi = 0;

    if (payload != nullptr && payload_length > 0) {
        out_frame.payload_len = static_cast<uint8_t>(
            std::min<size_t>(payload_length, sizeof(out_frame.payload))
        );
        std::memcpy(out_frame.payload, payload, out_frame.payload_len);
    }

    return true;
}

void ZigbeeRouterDriver::setNetworkReady(bool ready)
{
    network_ready_ = ready;
}

bool ZigbeeRouterDriver::isNetworkReady() const
{
    return network_ready_;
}
