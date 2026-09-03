#include "controllers/zigbee_controller.hpp"

#include "ezbee/bdb.h"
#include "esp_log.h"
#include "freertos/task.h"

#include "config/app_config.h"
#include "config/zigbee_config.h"

namespace {

constexpr char kTag[] = "ZigbeeController";
constexpr uint32_t kCommissioningRetryTaskStackSize = 3072;
constexpr UBaseType_t kCommissioningRetryTaskPriority = 4;

} // namespace

ZigbeeController *ZigbeeController::instance_ = nullptr;

ZigbeeController::ZigbeeController(ZigbeeRouterDriver &zigbee_driver,
                                   RouterPointController &router_point_controller)
    : zigbee_driver_(zigbee_driver),
      router_point_controller_(router_point_controller)
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

    ESP_LOGI(
        kTag,
        "app signal=%s (0x%04x) status=%u",
        signalName(signal_type),
        signal_type,
        status
    );

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
                ESP_LOGI(kTag, "factory-new router, starting network steering");
                esp_err_t err = zigbee_driver_.startCommissioning(EZB_BDB_MODE_NETWORK_STEERING);
                if (err != ESP_OK) {
                    ESP_LOGE(kTag, "network steering start failed: %s", esp_err_to_name(err));
                    scheduleCommissioningRetry(EZB_BDB_MODE_NETWORK_STEERING);
                }
            } else {
                ESP_LOGI(kTag, "router network restored");
                zigbee_driver_.setNetworkReady(true);
                commissioning_retry_task_running_ = false;
            }
        } else {
            ESP_LOGW(kTag, "BDB start/reboot failed, status=%u", status);
            zigbee_driver_.setNetworkReady(false);
            if (ezb_bdb_is_factory_new()) {
                scheduleCommissioningRetry(EZB_BDB_MODE_NETWORK_STEERING);
            } else {
                scheduleCommissioningRetry(EZB_BDB_MODE_INITIALIZATION);
            }
        }
        break;

    case EZB_BDB_SIGNAL_STEERING:
        if (status == EZB_BDB_STATUS_SUCCESS) {
            ESP_LOGI(kTag, "router network steering complete");
            zigbee_driver_.setNetworkReady(true);
            commissioning_retry_task_running_ = false;
        } else {
            ESP_LOGW(kTag, "router network steering failed, status=%u", status);
            zigbee_driver_.setNetworkReady(false);
            scheduleCommissioningRetry(EZB_BDB_MODE_NETWORK_STEERING);
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

    AppStatus status = router_point_controller_.enqueueFrameFromCallback(frame);
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

void ZigbeeController::commissioningRetryTaskEntry(void *arg)
{
    static_cast<ZigbeeController *>(arg)->commissioningRetryTask();
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
    ESP_LOGI(kTag, "initializing Zigbee router point");
    esp_err_t err = zigbee_driver_.init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Zigbee init failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }

    err = zigbee_driver_.setRxOnWhenIdle(true);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "failed to enable rx_on_when_idle: %s", esp_err_to_name(err));
    }

    ESP_LOGI(kTag, "registering router endpoint and navigation server cluster");
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

void ZigbeeController::commissioningRetryTask()
{
    vTaskDelay(pdMS_TO_TICKS(ZIGBEE_COMMISSIONING_RETRY_DELAY_MS));

    if (zigbee_driver_.isNetworkReady()) {
        commissioning_retry_task_running_ = false;
        vTaskDelete(nullptr);
        return;
    }

    const ezb_bdb_comm_mode_mask_t retry_mode = retry_commissioning_mode_;
    ESP_LOGI(
        kTag,
        "retrying BDB commissioning mode=%s; keep Zigbee2MQTT permit join open for new pairing",
        commissioningModeName(retry_mode)
    );

    esp_err_t err = zigbee_driver_.postCommissioning(retry_mode);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "BDB commissioning retry post failed: %s", esp_err_to_name(err));
        commissioning_retry_task_running_ = false;
        vTaskDelete(nullptr);
        return;
    }

    commissioning_retry_task_running_ = false;
    vTaskDelete(nullptr);
}

void ZigbeeController::scheduleCommissioningRetry(ezb_bdb_comm_mode_mask_t mode_mask)
{
    retry_commissioning_mode_ = mode_mask;
    if (commissioning_retry_task_running_) {
        return;
    }

    BaseType_t ok = xTaskCreate(
        commissioningRetryTaskEntry,
        "zb_comm_retry",
        kCommissioningRetryTaskStackSize,
        this,
        kCommissioningRetryTaskPriority,
        nullptr
    );
    if (ok == pdPASS) {
        commissioning_retry_task_running_ = true;
        ESP_LOGI(
            kTag,
            "BDB commissioning retry scheduled in %lums mode=%s",
            static_cast<unsigned long>(ZIGBEE_COMMISSIONING_RETRY_DELAY_MS),
            commissioningModeName(mode_mask)
        );
    } else {
        ESP_LOGE(kTag, "failed to create BDB commissioning retry task");
    }
}

const char *ZigbeeController::commissioningModeName(ezb_bdb_comm_mode_mask_t mode_mask) const
{
    switch (mode_mask) {
    case EZB_BDB_MODE_INITIALIZATION:
        return "initialization";
    case EZB_BDB_MODE_NETWORK_STEERING:
        return "network_steering";
    default:
        return "unknown";
    }
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
