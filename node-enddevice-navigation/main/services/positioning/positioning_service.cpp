#include "services/positioning/positioning_service.hpp"

#include <math.h>

#include "config/navigation_config.h"

float PositioningService::estimateDistanceCm(int8_t rssi) const
{
    const float exponent = (NAVIGATION_RSSI_AT_ONE_METER_DBM - static_cast<float>(rssi)) /
                           (10.0f * NAVIGATION_PATH_LOSS_EXPONENT);
    const float distance_m = powf(10.0f, exponent);
    return distance_m * 100.0f;
}

AppStatus PositioningService::calculatePosition(const NavAnchorSample *samples,
                                                size_t sample_count,
                                                Position2D &out_position) const
{
    if (samples == nullptr) {
        return AppStatus::kInvalidArgument;
    }
    if (sample_count == 0) {
        return AppStatus::kNoSamples;
    }
    if (sample_count < NAVIGATION_MIN_SAMPLES_FOR_POSITION) {
        return AppStatus::kNotEnoughSamples;
    }

    float weighted_x = 0.0f;
    float weighted_y = 0.0f;
    float total_weight = 0.0f;

    for (size_t i = 0; i < sample_count; ++i) {
        const float distance_cm = estimateDistanceCm(samples[i].rssi);
        const float clamped_distance = distance_cm < 1.0f ? 1.0f : distance_cm;
        const float weight = 1.0f / (clamped_distance * clamped_distance);

        weighted_x += samples[i].x_cm * weight;
        weighted_y += samples[i].y_cm * weight;
        total_weight += weight;
    }

    if (total_weight <= 0.0f) {
        return AppStatus::kInvalidPayload;
    }

    out_position.x_cm = weighted_x / total_weight;
    out_position.y_cm = weighted_y / total_weight;
    out_position.confidence = sample_count >= 3 ? 1.0f : 0.55f;
    return AppStatus::kOk;
}

