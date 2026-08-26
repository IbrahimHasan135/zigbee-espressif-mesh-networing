#include "controllers/zigbee_controller.hpp"

#include "ezbee/bdb.h"
#include "esp_log.h"
#include "freertos/task.h"

#include "config/app_config.h"

namespace {

constexpr char kTag[] = "ZigbeeController";

} // namespace

ZigbeeController *ZigbeeController::instance_ = nullptr;

ZigbeeController::ZigbeeController(ZigbeeEndDeviceDriver &zigbee_driver,
                                   NavigationController &navigation_controller)
    : zigbee_driver_(zigbee_driver),
      navigation_controller_(navigation_controller)
{
    instance_ = this;
}

esp_err_t ZigbeeController::start()
{
    BaseType_t ok = xTaskCreate(
        taskEntry,
        "zigbee_main",
        ZIGBEE_CONTROLLER_TASK_STACK_SIZE,
        this,
        ZIGBEE_CONTROLLER_TASK_PRIORITY,
        nullptr
    );
    if (ok != pdPASS) {
        ESP_LOGE(kTag, "failed to create zigbee_main task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(kTag, "zigbee_main task started");
    return ESP_OK;
}

bool ZigbeeController::handleAppSignal(const ezb_app_signal_t *signal)
{
    if (signal == nullptr) {
        ESP_LOGW(kTag, "received empty Zigbee app signal");
        return false;
    }

    const ezb_app_signal_type_t signal_type = ezb_app_signal_get_type(signal);
    const void *params = ezb_app_signal_get_params(signal);
    uint8_t status = 0;
    if (params != nullptr) {
        status = static_cast<const ezb_bdb_signal_simple_params_t *>(params)->status;
    }

    ESP_LOGI(kTag,
             "app signal=%s (0x%04x) status=%u",
             signalName(signal_type),
             signal_type,
             status);

    switch (signal_type) {
    case EZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(kTag, "starting BDB commissioning initialization");
        {
            esp_err_t err = zigbee_driver_.startCommissioning(EZB_BDB_MODE_INITIALIZATION);
            if (err != ESP_OK) {
                ESP_LOGE(kTag, "BDB initialization start failed: %s", esp_err_to_name(err));
            }
        }
        break;

    case EZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case EZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (status == EZB_BDB_STATUS_SUCCESS) {
            if (ezb_bdb_is_factory_new()) {
                ESP_LOGI(kTag, "factory-new end device, starting network steering");
                esp_err_t err = zigbee_driver_.startCommissioning(EZB_BDB_MODE_NETWORK_STEERING);
                if (err != ESP_OK) {
                    ESP_LOGE(kTag, "network steering start failed: %s", esp_err_to_name(err));
                }
            } else {
                ESP_LOGI(kTag, "network restored, navigation can start");
                zigbee_driver_.setNetworkReady(true);
            }
        } else {
            ESP_LOGW(kTag, "BDB start/reboot failed, status=%u", status);
        }
        break;

    case EZB_BDB_SIGNAL_STEERING:
        if (status == EZB_BDB_STATUS_SUCCESS) {
            ESP_LOGI(kTag, "network steering complete");
            zigbee_driver_.setNetworkReady(true);
        } else {
            ESP_LOGW(kTag, "network steering failed, status=%u", status);
            zigbee_driver_.setNetworkReady(false);
        }
        break;

    default:
        break;
    }

    return true;
}

ezb_zcl_status_t ZigbeeController::handleCustomClusterCommand(const ezb_zcl_cmd_hdr_t *header,
                                                              const uint8_t *payload,
                                                              uint16_t payload_length)
{
    ZigbeeFrame frame = {};
    if (!zigbee_driver_.decodeCustomCommand(header, payload, payload_length, frame)) {
        return EZB_ZCL_STATUS_SUCCESS;
    }

    AppStatus status = navigation_controller_.enqueueFrameFromCallback(frame);
    if (status != AppStatus::kOk) {
        ESP_LOGW(
            kTag,
            "failed to enqueue Zigbee frame, src=0x%04x cluster=0x%04x cmd=0x%02x status=%d",
            frame.src_short_addr,
            frame.cluster_id,
            frame.command_id,
            static_cast<int>(status)
        );
    }

    return EZB_ZCL_STATUS_SUCCESS;
}

ZigbeeController *ZigbeeController::instance()
{
    return instance_;
}

void ZigbeeController::taskEntry(void *arg)
{
    static_cast<ZigbeeController *>(arg)->task();
}

bool ZigbeeController::appSignalHandler(const ezb_app_signal_t *signal)
{
    ZigbeeController *controller = ZigbeeController::instance();
    if (controller == nullptr) {
        return false;
    }
    return controller->handleAppSignal(signal);
}

ezb_zcl_status_t ZigbeeController::customClusterCommandHandler(const ezb_zcl_cmd_hdr_t *header,
                                                               const uint8_t *payload,
                                                               uint16_t payload_length)
{
    ZigbeeController *controller = ZigbeeController::instance();
    if (controller == nullptr) {
        return EZB_ZCL_STATUS_SUCCESS;
    }
    return controller->handleCustomClusterCommand(header, payload, payload_length);
}

void ZigbeeController::task()
{
    ESP_LOGI(kTag, "initializing Zigbee sleepy end device");
    esp_err_t err = zigbee_driver_.init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Zigbee init failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }

    err = zigbee_driver_.setRxOnWhenIdle(false);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "failed to disable rx_on_when_idle: %s", esp_err_to_name(err));
    }

    err = zigbee_driver_.configureSleep(true, 20);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "failed to enable Zigbee sleep threshold: %s", esp_err_to_name(err));
    }

    ESP_LOGI(kTag, "registering Zigbee endpoint and custom navigation cluster");
    err = zigbee_driver_.registerDevice();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "device registration failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }

    err = zigbee_driver_.registerAppSignalHandler(appSignalHandler);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "app signal handler registration failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }

    err = zigbee_driver_.registerCustomClusterHandler(customClusterCommandHandler);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "custom cluster handler registration failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(kTag, "starting Zigbee stack");
    err = zigbee_driver_.start(false);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Zigbee start failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(kTag, "entering Zigbee main loop");
    zigbee_driver_.runMainLoop();
    vTaskDelete(nullptr);
}

const char *ZigbeeController::signalName(uint32_t signal) const
{
    switch (signal) {
    case EZB_ZDO_SIGNAL_SKIP_STARTUP:
        return "skip_startup";
    case EZB_BDB_SIGNAL_DEVICE_FIRST_START:
        return "device_first_start";
    case EZB_BDB_SIGNAL_DEVICE_REBOOT:
        return "device_reboot";
    case EZB_BDB_SIGNAL_STEERING:
        return "steering";
    default:
        return "unknown";
    }
}
