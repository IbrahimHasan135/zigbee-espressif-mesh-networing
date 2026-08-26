#include "services/navigation/navigation_service.hpp"

#include "common/byte_utils.h"
#include "config/zigbee_config.h"

NavigationService::NavigationService(ZigbeeEndDeviceDriver &zigbee_driver)
    : zigbee_driver_(zigbee_driver)
{
}

AppStatus NavigationService::beginCycle(uint16_t sequence_id)
{
    active_sequence_id_ = sequence_id;
    sample_count_ = 0;
    cycle_active_ = true;
    return AppStatus::kOk;
}

esp_err_t NavigationService::sendPing()
{
    if (!cycle_active_) {
        return ESP_ERR_INVALID_STATE;
    }
    return zigbee_driver_.sendBroadcastPing(active_sequence_id_);
}

AppStatus NavigationService::acceptFrame(const ZigbeeFrame &frame, uint32_t received_at_ms)
{
    if (!cycle_active_) {
        return AppStatus::kIgnored;
    }

    if (sample_count_ >= NAVIGATION_MAX_SAMPLES) {
        return AppStatus::kSampleBufferFull;
    }

    NavAnchorSample sample = {};
    AppStatus status = decodeResponse(frame, sample);
    if (status != AppStatus::kOk) {
        return status;
    }

    sample.received_at_ms = received_at_ms;
    samples_[sample_count_] = sample;
    sample_count_++;
    return AppStatus::kOk;
}

NavigationCycleResult NavigationService::finishCycle(const Position2D *position)
{
    NavigationCycleResult result = {};
    result.sequence_id = active_sequence_id_;
    result.sample_count = sample_count_;
    result.status = sample_count_ == 0 ? AppStatus::kNoSamples : AppStatus::kOk;
    if (position != nullptr) {
        result.position = *position;
    }
    cycle_active_ = false;
    return result;
}

uint16_t NavigationService::activeSequenceId() const
{
    return active_sequence_id_;
}

size_t NavigationService::sampleCount() const
{
    return sample_count_;
}

const NavAnchorSample *NavigationService::samples() const
{
    return samples_;
}

AppStatus NavigationService::decodeResponse(const ZigbeeFrame &frame, NavAnchorSample &out_sample) const
{
    if (frame.cluster_id != CUSTOM_NAV_CLUSTER_ID) {
        return AppStatus::kInvalidCluster;
    }
    if (frame.command_id != CMD_PING_RSP) {
        return AppStatus::kInvalidCommand;
    }

    if (frame.payload_len == 4) {
        out_sample.sequence_id = active_sequence_id_;
        out_sample.router_id = frame.src_short_addr;
        out_sample.x_cm = static_cast<float>(readU16Be(&frame.payload[0]));
        out_sample.y_cm = static_cast<float>(readU16Be(&frame.payload[2]));
    } else if (frame.payload_len >= 8) {
        const uint16_t sequence_id = readU16Be(&frame.payload[0]);
        if (sequence_id != active_sequence_id_) {
            return AppStatus::kSequenceMismatch;
        }
        out_sample.sequence_id = sequence_id;
        out_sample.router_id = readU16Be(&frame.payload[2]);
        out_sample.x_cm = static_cast<float>(readU16Be(&frame.payload[4]));
        out_sample.y_cm = static_cast<float>(readU16Be(&frame.payload[6]));
    } else {
        return AppStatus::kInvalidPayload;
    }

    out_sample.router_short_addr = frame.src_short_addr;
    out_sample.rssi = frame.rssi;
    out_sample.lqi = frame.lqi;
    return AppStatus::kOk;
}

