#pragma once

#include "esp_err.h"

enum class AppStatus {
    kOk = 0,
    kIgnored,
    kInvalidArgument,
    kInvalidCluster,
    kInvalidCommand,
    kInvalidPayload,
    kSequenceMismatch,
    kSampleBufferFull,
    kNetworkNotReady,
    kQueueFull,
    kNoSamples,
    kNotEnoughSamples,
    kDriverError,
};

inline esp_err_t appStatusToEspErr(AppStatus status)
{
    switch (status) {
    case AppStatus::kOk:
    case AppStatus::kIgnored:
        return ESP_OK;
    case AppStatus::kInvalidArgument:
    case AppStatus::kInvalidCluster:
    case AppStatus::kInvalidCommand:
    case AppStatus::kInvalidPayload:
    case AppStatus::kSequenceMismatch:
        return ESP_ERR_INVALID_ARG;
    case AppStatus::kSampleBufferFull:
    case AppStatus::kQueueFull:
        return ESP_ERR_NO_MEM;
    case AppStatus::kNetworkNotReady:
        return ESP_ERR_INVALID_STATE;
    case AppStatus::kNoSamples:
    case AppStatus::kNotEnoughSamples:
        return ESP_ERR_NOT_FOUND;
    case AppStatus::kDriverError:
    default:
        return ESP_FAIL;
    }
}

