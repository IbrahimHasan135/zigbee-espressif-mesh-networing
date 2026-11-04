#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "common/nav_types.h"
#include "services/navigation/navigation_service.hpp"
#include "services/positioning/positioning_service.hpp"
#include "services/power/power_service.hpp"

class NavigationController {
public:
    NavigationController(NavigationService &navigation_service,
                         PositioningService &positioning_service,
                         PowerService &power_service);

    esp_err_t init();
    esp_err_t start();
    AppStatus enqueueFrameFromCallback(const ZigbeeFrame &frame);

private:
    static void cycleTaskEntry(void *arg);
    static void eventTaskEntry(void *arg);

    void cycleTask();
    void eventTask();
    const char *statusName(AppStatus status) const;
    const char *powerDecisionName(PowerDecision decision) const;

    NavigationService &navigation_service_;
    PositioningService &positioning_service_;
    PowerService &power_service_;
    QueueHandle_t frame_queue_ = nullptr;
    SemaphoreHandle_t service_mutex_ = nullptr;
    uint16_t sequence_id_ = 0;
};
