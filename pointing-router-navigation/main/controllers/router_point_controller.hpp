#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "common/nav_types.h"
#include "drivers/status/status_led_driver.hpp"
#include "services/router_point/router_point_service.hpp"

struct RouterPointDiagnostics {
    uint32_t ping_received_count = 0;
    uint32_t ping_accepted_count = 0;
    uint32_t response_sent_count = 0;
    uint32_t response_failed_count = 0;
    uint16_t last_sender_short_addr = 0;
    uint16_t last_sequence_id = 0;
    AppStatus last_error = AppStatus::kOk;
};

class RouterPointController {
public:
    RouterPointController(RouterPointService &router_point_service,
                          StatusLedDriver &status_led_driver);

    esp_err_t init();
    esp_err_t start();
    AppStatus enqueueFrameFromCallback(const ZigbeeFrame &frame);
    const RouterPointDiagnostics &diagnostics() const;

private:
    static void eventTaskEntry(void *arg);
    static void diagnosticTaskEntry(void *arg);

    void eventTask();
    void diagnosticTask();
    void updateDiagnostics(const ZigbeeFrame &frame, const RouterPointProcessResult &result);
    const char *statusName(AppStatus status) const;

    RouterPointService &router_point_service_;
    StatusLedDriver &status_led_driver_;
    QueueHandle_t frame_queue_ = nullptr;
    SemaphoreHandle_t diagnostics_mutex_ = nullptr;
    RouterPointDiagnostics diagnostics_ = {};
};
