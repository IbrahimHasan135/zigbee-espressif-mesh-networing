#pragma once

#include <stdint.h>

#include "esp_err.h"

#include "common/nav_types.h"
#include "drivers/power/power_driver.hpp"
#include "drivers/storage/storage_driver.hpp"

enum class PowerDecision {
    kStayAwake = 0,
    kRetrySoon,
    kDeepSleep,
};

struct PowerPlan {
    PowerDecision decision = PowerDecision::kStayAwake;
    uint32_t delay_ms = 0;
};

class PowerService {
public:
    PowerService(PowerDriver &power_driver, StorageDriver &storage_driver);

    esp_err_t init();
    const NodeConfig &config() const;
    PowerPlan makePlan(const NavigationCycleResult &cycle_result) const;
    esp_err_t executePlan(const PowerPlan &plan);

private:
    PowerDriver &power_driver_;
    StorageDriver &storage_driver_;
    NodeConfig config_ = {};
};
