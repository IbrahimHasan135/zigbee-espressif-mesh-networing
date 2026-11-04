#pragma once

#include <stddef.h>

#include "esp_err.h"

class SerialTelemetryDriver {
public:
    esp_err_t init();
    esp_err_t writeLine(const char *line, size_t length);

private:
    bool initialized_ = false;
};
