#include "services/power/power_service.hpp"

#include "config/navigation_config.h"

PowerService::PowerService(PowerDriver &power_driver, StorageDriver &storage_driver)
    : power_driver_(power_driver), storage_driver_(storage_driver)
{
}

esp_err_t PowerService::init()
{
    NodeConfig loaded_config = {};
    esp_err_t err = storage_driver_.readNodeConfig(loaded_config);
    if (err != ESP_OK) {
        return err;
    }

    if (loaded_config.listen_window_ms == 0) {
        loaded_config.listen_window_ms = NAVIGATION_LISTEN_WINDOW_MS;
    }
    if (loaded_config.sleep_interval_ms == 0) {
        loaded_config.sleep_interval_ms = NAVIGATION_SLEEP_INTERVAL_MS;
    }
    if (loaded_config.retry_interval_ms == 0) {
        loaded_config.retry_interval_ms = NAVIGATION_RETRY_INTERVAL_MS;
    }

    config_ = loaded_config;
    return ESP_OK;
}

const NodeConfig &PowerService::config() const
{
    return config_;
}

PowerPlan PowerService::makePlan(const NavigationCycleResult &cycle_result) const
{
    PowerPlan plan = {};
    if (cycle_result.status == AppStatus::kOk) {
        plan.decision = PowerDecision::kDeepSleep;
        plan.delay_ms = config_.sleep_interval_ms;
        return plan;
    }

    if (cycle_result.status == AppStatus::kNoSamples ||
        cycle_result.status == AppStatus::kNotEnoughSamples) {
        plan.decision = PowerDecision::kRetrySoon;
        plan.delay_ms = config_.retry_interval_ms;
        return plan;
    }

    plan.decision = PowerDecision::kStayAwake;
    plan.delay_ms = config_.retry_interval_ms;
    return plan;
}

esp_err_t PowerService::executePlan(const PowerPlan &plan)
{
    if (plan.decision == PowerDecision::kDeepSleep) {
        return power_driver_.enterDeepSleep(plan.delay_ms);
    }
    return ESP_OK;
}

