#include "controllers/router_point_controller.hpp"

#include "esp_log.h"
#include "freertos/task.h"

#include "config/app_config.h"

namespace {

constexpr char kTag[] = "RouterPointController";
constexpr uint32_t kQueueDepth = 16;

} // namespace

RouterPointController::RouterPointController(RouterPointService &router_point_service,
                                             StatusLedDriver &status_led_driver)
    : router_point_service_(router_point_service),
      status_led_driver_(status_led_driver)
{
}

esp_err_t RouterPointController::init()
{
    frame_queue_ = xQueueCreate(kQueueDepth, sizeof(ZigbeeFrame));
    if (frame_queue_ == nullptr) {
        ESP_LOGE(kTag, "failed to create router frame queue");
        return ESP_ERR_NO_MEM;
    }

    diagnostics_mutex_ = xSemaphoreCreateMutex();
    if (diagnostics_mutex_ == nullptr) {
        ESP_LOGE(kTag, "failed to create diagnostics mutex");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(kTag, "router frame queue ready, depth=%lu", static_cast<unsigned long>(kQueueDepth));
    return ESP_OK;
}

esp_err_t RouterPointController::start()
{
    if (frame_queue_ == nullptr || diagnostics_mutex_ == nullptr) {
        ESP_LOGE(kTag, "cannot start router controller before init");
        return ESP_ERR_INVALID_STATE;
    }

    BaseType_t ok = xTaskCreate(
        eventTaskEntry,
        "router_event",
        ROUTER_EVENT_TASK_STACK_SIZE,
        this,
        ROUTER_EVENT_TASK_PRIORITY,
        nullptr
    );
    if (ok != pdPASS) {
        ESP_LOGE(kTag, "failed to create router_event task");
        return ESP_ERR_NO_MEM;
    }

    ok = xTaskCreate(
        diagnosticTaskEntry,
        "router_diag",
        ROUTER_DIAGNOSTIC_TASK_STACK_SIZE,
        this,
        ROUTER_DIAGNOSTIC_TASK_PRIORITY,
        nullptr
    );
    if (ok != pdPASS) {
        ESP_LOGE(kTag, "failed to create router_diag task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(kTag, "router tasks started");
    return ESP_OK;
}

AppStatus RouterPointController::enqueueFrameFromCallback(const ZigbeeFrame &frame)
{
    if (frame_queue_ == nullptr) {
        return AppStatus::kInvalidArgument;
    }

    if (xQueueSend(frame_queue_, &frame, 0) != pdTRUE) {
        return AppStatus::kQueueFull;
    }

    return AppStatus::kOk;
}

const RouterPointDiagnostics &RouterPointController::diagnostics() const
{
    return diagnostics_;
}

void RouterPointController::eventTaskEntry(void *arg)
{
    static_cast<RouterPointController *>(arg)->eventTask();
}

void RouterPointController::diagnosticTaskEntry(void *arg)
{
    static_cast<RouterPointController *>(arg)->diagnosticTask();
}

void RouterPointController::eventTask()
{
    ESP_LOGI(kTag, "router event task running");

    ZigbeeFrame frame = {};
    while (true) {
        if (xQueueReceive(frame_queue_, &frame, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        status_led_driver_.pulsePingReceived();
        RouterPointProcessResult result = router_point_service_.handleIncomingFrame(frame);
        updateDiagnostics(frame, result);

        if (result.status == AppStatus::kOk) {
            status_led_driver_.pulseResponseSent();
            ESP_LOGI(
                kTag,
                "PING_RSP sent, dst=0x%04x sequence=%u payload=%s",
                result.sender_short_addr,
                result.sequence_id,
                result.has_sequence_id ? "full" : "legacy"
            );
        } else if (result.status != AppStatus::kInvalidCluster &&
                   result.status != AppStatus::kInvalidCommand) {
            if (result.status == AppStatus::kDriverError) {
                ESP_LOGW(
                    kTag,
                    "frame rejected, src=0x%04x cluster=0x%04x cmd=0x%02x status=%s err=%s",
                    frame.src_short_addr,
                    frame.cluster_id,
                    frame.command_id,
                    statusName(result.status),
                    esp_err_to_name(result.driver_error)
                );
            } else {
                ESP_LOGW(
                    kTag,
                    "frame rejected, src=0x%04x cluster=0x%04x cmd=0x%02x status=%s",
                    frame.src_short_addr,
                    frame.cluster_id,
                    frame.command_id,
                    statusName(result.status)
                );
            }
        }
    }
}

void RouterPointController::diagnosticTask()
{
    ESP_LOGI(kTag, "router diagnostic task running");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(ROUTER_DIAGNOSTIC_INTERVAL_MS));

        xSemaphoreTake(diagnostics_mutex_, portMAX_DELAY);
        RouterPointDiagnostics snapshot = diagnostics_;
        xSemaphoreGive(diagnostics_mutex_);

        ESP_LOGI(
            kTag,
            "diag ping_rx=%lu accepted=%lu rsp_ok=%lu rsp_fail=%lu last_src=0x%04x last_seq=%u last_error=%s",
            static_cast<unsigned long>(snapshot.ping_received_count),
            static_cast<unsigned long>(snapshot.ping_accepted_count),
            static_cast<unsigned long>(snapshot.response_sent_count),
            static_cast<unsigned long>(snapshot.response_failed_count),
            snapshot.last_sender_short_addr,
            snapshot.last_sequence_id,
            statusName(snapshot.last_error)
        );
    }
}

void RouterPointController::updateDiagnostics(const ZigbeeFrame &frame,
                                              const RouterPointProcessResult &result)
{
    xSemaphoreTake(diagnostics_mutex_, portMAX_DELAY);
    diagnostics_.ping_received_count++;
    diagnostics_.last_sender_short_addr = frame.src_short_addr;
    diagnostics_.last_sequence_id = result.sequence_id;
    diagnostics_.last_error = result.status;

    if (result.status == AppStatus::kOk) {
        diagnostics_.ping_accepted_count++;
        diagnostics_.response_sent_count++;
    } else if (result.status == AppStatus::kDriverError ||
               result.status == AppStatus::kNetworkNotReady) {
        diagnostics_.response_failed_count++;
    }
    xSemaphoreGive(diagnostics_mutex_);
}

const char *RouterPointController::statusName(AppStatus status) const
{
    switch (status) {
    case AppStatus::kOk:
        return "ok";
    case AppStatus::kIgnored:
        return "ignored";
    case AppStatus::kInvalidArgument:
        return "invalid_argument";
    case AppStatus::kInvalidCluster:
        return "invalid_cluster";
    case AppStatus::kInvalidCommand:
        return "invalid_command";
    case AppStatus::kInvalidPayload:
        return "invalid_payload";
    case AppStatus::kResponseDisabled:
        return "response_disabled";
    case AppStatus::kNetworkNotReady:
        return "network_not_ready";
    case AppStatus::kQueueFull:
        return "queue_full";
    case AppStatus::kDriverError:
        return "driver_error";
    default:
        return "unknown";
    }
}
