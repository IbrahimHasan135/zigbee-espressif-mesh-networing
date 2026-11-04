#include "controllers/navigation_controller.hpp"

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/task.h"

#include "common/byte_utils.h"
#include "common/nav_types.h"
#include "config/app_config.h"

namespace {

constexpr char kTag[] = "NavigationController";
constexpr uint32_t kQueueDepth = 16;

} // namespace

NavigationController::NavigationController(NavigationService &navigation_service,
                                           PositioningService &positioning_service,
                                           PowerService &power_service)
    : navigation_service_(navigation_service),
      positioning_service_(positioning_service),
      power_service_(power_service)
{
}

esp_err_t NavigationController::init()
{
    frame_queue_ = xQueueCreate(kQueueDepth, sizeof(ZigbeeFrame));
    if (frame_queue_ == nullptr) {
        ESP_LOGE(kTag, "failed to create Zigbee frame queue");
        return ESP_ERR_NO_MEM;
    }

    service_mutex_ = xSemaphoreCreateMutex();
    if (service_mutex_ == nullptr) {
        ESP_LOGE(kTag, "failed to create service mutex");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(kTag, "frame queue ready, depth=%lu", static_cast<unsigned long>(kQueueDepth));
    return ESP_OK;
}

esp_err_t NavigationController::start()
{
    if (frame_queue_ == nullptr || service_mutex_ == nullptr) {
        ESP_LOGE(kTag, "cannot start, queue or mutex is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    BaseType_t ok = xTaskCreate(
        cycleTaskEntry,
        "navigation_cycle",
        NAVIGATION_CYCLE_TASK_STACK_SIZE,
        this,
        NAVIGATION_CYCLE_TASK_PRIORITY,
        nullptr
    );
    if (ok != pdPASS) {
        ESP_LOGE(kTag, "failed to create navigation_cycle task");
        return ESP_ERR_NO_MEM;
    }

    ok = xTaskCreate(
        eventTaskEntry,
        "navigation_event",
        NAVIGATION_EVENT_TASK_STACK_SIZE,
        this,
        NAVIGATION_EVENT_TASK_PRIORITY,
        nullptr
    );
    if (ok != pdPASS) {
        ESP_LOGE(kTag, "failed to create navigation_event task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(kTag, "navigation tasks started");
    return ESP_OK;
}

AppStatus NavigationController::enqueueFrameFromCallback(const ZigbeeFrame &frame)
{
    if (frame_queue_ == nullptr) {
        return AppStatus::kInvalidArgument;
    }

    if (xQueueSend(frame_queue_, &frame, 0) != pdTRUE) {
        return AppStatus::kQueueFull;
    }

    return AppStatus::kOk;
}

void NavigationController::cycleTaskEntry(void *arg)
{
    static_cast<NavigationController *>(arg)->cycleTask();
}

void NavigationController::eventTaskEntry(void *arg)
{
    static_cast<NavigationController *>(arg)->eventTask();
}

void NavigationController::cycleTask()
{
    const NodeConfig &config = power_service_.config();
    ESP_LOGI(
        kTag,
        "cycle task running, node_id=0x%04x listen=%lums sleep=%lums retry=%lums",
        config.node_id,
        static_cast<unsigned long>(config.listen_window_ms),
        static_cast<unsigned long>(config.sleep_interval_ms),
        static_cast<unsigned long>(config.retry_interval_ms)
    );

    while (true) {
        sequence_id_++;
        if (sequence_id_ == 0) {
            sequence_id_ = 1;
        }

        xSemaphoreTake(service_mutex_, portMAX_DELAY);
        navigation_service_.beginCycle(sequence_id_);
        xSemaphoreGive(service_mutex_);
        ESP_LOGI(kTag, "navigation cycle started, sequence=%u", sequence_id_);

        esp_err_t err = navigation_service_.sendPing();
        if (err != ESP_OK) {
            ESP_LOGW(
                kTag,
                "send ping failed, sequence=%u err=%s; retry in %lums",
                sequence_id_,
                esp_err_to_name(err),
                static_cast<unsigned long>(config.retry_interval_ms)
            );
            xSemaphoreTake(service_mutex_, portMAX_DELAY);
            NavigationCycleResult result = navigation_service_.finishCycle(nullptr);
            xSemaphoreGive(service_mutex_);
            (void)result;
            vTaskDelay(pdMS_TO_TICKS(config.retry_interval_ms));
            continue;
        }

        ESP_LOGI(kTag, "ping broadcast sent, sequence=%u", sequence_id_);
        vTaskDelay(pdMS_TO_TICKS(config.listen_window_ms));

        xSemaphoreTake(service_mutex_, portMAX_DELAY);
        Position2D position = {};
        AppStatus position_status = positioning_service_.calculatePosition(
            navigation_service_.samples(),
            navigation_service_.sampleCount(),
            position
        );

        NavigationCycleResult result = navigation_service_.finishCycle(
            position_status == AppStatus::kOk ? &position : nullptr
        );
        if (position_status != AppStatus::kOk) {
            result.status = position_status;
        }
        xSemaphoreGive(service_mutex_);

        if (result.status == AppStatus::kOk) {
            ESP_LOGI(
                kTag,
                "position calculated, sequence=%u samples=%u x=%.2fcm y=%.2fcm confidence=%.2f",
                result.sequence_id,
                static_cast<unsigned>(result.sample_count),
                result.position.x_cm,
                result.position.y_cm,
                result.position.confidence
            );
        } else {
            if (result.status != AppStatus::kNotEnoughSamples) {
                ESP_LOGW(
                    kTag,
                    "cycle ended without position, sequence=%u samples=%u status=%s",
                    result.sequence_id,
                    static_cast<unsigned>(result.sample_count),
                    statusName(result.status)
                );
            }
        }

        const PowerPlan plan = power_service_.makePlan(result);
        ESP_LOGI(
            kTag,
            "power plan, decision=%s delay=%lums",
            powerDecisionName(plan.decision),
            static_cast<unsigned long>(plan.delay_ms)
        );

        if (plan.decision == PowerDecision::kDeepSleep) {
            ESP_LOGI(kTag, "entering deep sleep");
            err = power_service_.executePlan(plan);
            ESP_LOGE(kTag, "deep sleep request failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(config.retry_interval_ms));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(plan.delay_ms));
    }
}

void NavigationController::eventTask()
{
    ESP_LOGI(kTag, "event task running");

    ZigbeeFrame frame = {};
    while (true) {
        if (xQueueReceive(frame_queue_, &frame, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        const uint32_t received_at_ms = pdTICKS_TO_MS(xTaskGetTickCount());
        xSemaphoreTake(service_mutex_, portMAX_DELAY);
        AppStatus status = navigation_service_.acceptFrame(frame, received_at_ms);
        const uint16_t active_sequence_id = navigation_service_.activeSequenceId();
        const size_t sample_count = navigation_service_.sampleCount();
        uint16_t accepted_sequence_id = active_sequence_id;
        if (status == AppStatus::kOk && sample_count > 0) {
            accepted_sequence_id = navigation_service_.samples()[sample_count - 1].sequence_id;
        }
        const uint16_t accepted_delay_cycles = static_cast<uint16_t>(
            active_sequence_id - accepted_sequence_id
        );
        xSemaphoreGive(service_mutex_);
        if (status == AppStatus::kOk) {
            ESP_LOGI(
                kTag,
                "sample accepted, active_seq=%u sample_seq=%u delay_cycles=%u router=0x%04x rssi=%d samples=%u",
                active_sequence_id,
                accepted_sequence_id,
                accepted_delay_cycles,
                frame.src_short_addr,
                frame.rssi,
                static_cast<unsigned>(sample_count)
            );
        } else if (status == AppStatus::kSequenceMismatch && frame.payload_len >= 2) {
            ESP_LOGW(
                kTag,
                "frame rejected, src=0x%04x cluster=0x%04x cmd=0x%02x status=%s active_seq=%u received_seq=%u",
                frame.src_short_addr,
                frame.cluster_id,
                frame.command_id,
                statusName(status),
                active_sequence_id,
                readU16Be(&frame.payload[0])
            );
        } else if (status != AppStatus::kIgnored &&
                   status != AppStatus::kInvalidCluster &&
                   status != AppStatus::kInvalidCommand) {
            ESP_LOGW(
                kTag,
                "frame rejected, src=0x%04x cluster=0x%04x cmd=0x%02x status=%s",
                frame.src_short_addr,
                frame.cluster_id,
                frame.command_id,
                statusName(status)
            );
        }
    }
}

const char *NavigationController::statusName(AppStatus status) const
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
    case AppStatus::kSequenceMismatch:
        return "sequence_mismatch";
    case AppStatus::kSampleBufferFull:
        return "sample_buffer_full";
    case AppStatus::kNetworkNotReady:
        return "network_not_ready";
    case AppStatus::kQueueFull:
        return "queue_full";
    case AppStatus::kNoSamples:
        return "no_samples";
    case AppStatus::kNotEnoughSamples:
        return "not_enough_samples";
    case AppStatus::kDriverError:
        return "driver_error";
    default:
        return "unknown";
    }
}

const char *NavigationController::powerDecisionName(PowerDecision decision) const
{
    switch (decision) {
    case PowerDecision::kStayAwake:
        return "stay_awake";
    case PowerDecision::kRetrySoon:
        return "retry_soon";
    case PowerDecision::kDeepSleep:
        return "deep_sleep";
    default:
        return "unknown";
    }
}
