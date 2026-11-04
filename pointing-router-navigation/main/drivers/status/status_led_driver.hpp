#pragma once

#include "esp_err.h"

class StatusLedDriver {
public:
    esp_err_t init();
    void setBooting();
    void setJoined();
    void pulsePingReceived();
    void pulseResponseSent();

private:
    bool initialized_ = false;
};
