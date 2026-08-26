#include "services/navigation_protocol/navigation_protocol_service.hpp"

#include "common/byte_utils.h"

AppStatus NavigationProtocolService::decodePingRequest(const ZigbeeFrame &frame,
                                                       PingRequest &out_request) const
{
    if (frame.cluster_id != CUSTOM_NAV_CLUSTER_ID) {
        return AppStatus::kInvalidCluster;
    }
    if (frame.command_id != CMD_PING_REQ) {
        return AppStatus::kInvalidCommand;
    }
    if (frame.payload_len != 0 && frame.payload_len < PING_REQ_SEQUENCE_LEN) {
        return AppStatus::kInvalidPayload;
    }

    out_request = {};
    out_request.sender_short_addr = frame.src_short_addr;
    if (frame.payload_len >= PING_REQ_SEQUENCE_LEN) {
        out_request.sequence_id = readU16Be(&frame.payload[0]);
        out_request.has_sequence_id = true;
    }

    return AppStatus::kOk;
}

AppStatus NavigationProtocolService::encodePingResponse(const PingRequest &request,
                                                        const RouterPointConfig &config,
                                                        PingResponsePayload &out_payload) const
{
    if (!config.response_enabled) {
        return AppStatus::kResponseDisabled;
    }

    out_payload = {};
    if (request.has_sequence_id) {
        writeU16Be(&out_payload.bytes[0], request.sequence_id);
        writeU16Be(&out_payload.bytes[2], config.router_id);
        writeU16Be(&out_payload.bytes[4], config.x_cm);
        writeU16Be(&out_payload.bytes[6], config.y_cm);
        out_payload.len = PING_RSP_FULL_LEN;
        return AppStatus::kOk;
    }

    writeU16Be(&out_payload.bytes[0], config.x_cm);
    writeU16Be(&out_payload.bytes[2], config.y_cm);
    out_payload.len = PING_RSP_LEGACY_LEN;
    return AppStatus::kOk;
}
