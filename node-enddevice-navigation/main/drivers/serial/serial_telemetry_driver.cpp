#include "drivers/serial/serial_telemetry_driver.hpp"

#include "config/navigation_config.h"
#include "freertos/FreeRTOS.h"

esp_err_t SerialTelemetryDriver::init()
{
    uart_config_t config = {};
    config.baud_rate = NAVIGATION_TELEMETRY_BAUD_RATE;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_DEFAULT;

    esp_err_t err = uart_param_config(NAVIGATION_TELEMETRY_UART_PORT, &config);
    if (err == ESP_OK) {
        err = uart_set_pin(
            NAVIGATION_TELEMETRY_UART_PORT,
            NAVIGATION_TELEMETRY_TX_GPIO,
            UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE);
    }
    if (err == ESP_OK) {
        err = uart_driver_install(
            NAVIGATION_TELEMETRY_UART_PORT,
            NAVIGATION_TELEMETRY_RX_BUFFER_SIZE,
            NAVIGATION_TELEMETRY_TX_BUFFER_SIZE,
            0,
            nullptr,
            0);
    }

    initialized_ = (err == ESP_OK);
    return err;
}

esp_err_t SerialTelemetryDriver::writeLine(const char *line, size_t length)
{
    if (!initialized_ || line == nullptr || length == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    const int written = uart_write_bytes(
        NAVIGATION_TELEMETRY_UART_PORT, line, length);
    if (written != static_cast<int>(length)) {
        return ESP_FAIL;
    }
    return uart_wait_tx_done(
        NAVIGATION_TELEMETRY_UART_PORT,
        pdMS_TO_TICKS(NAVIGATION_TELEMETRY_WRITE_TIMEOUT_MS));
}
