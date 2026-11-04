#include "services/router_point/router_point_service.hpp"

RouterPointService::RouterPointService(ZigbeeRouterDriver &zigbee_driver,
                                       RouterConfigService &config_service,
                                       NavigationProtocolService &protocol_service)
    : zigbee_driver_(zigbee_driver),
      config_service_(config_service),
      protocol_service_(protocol_service)
{
}

RouterPointProcessResult RouterPointService::handleIncomingFrame(const ZigbeeFrame &frame)
{
    RouterPointProcessResult result = {};
    result.sender_short_addr = frame.src_short_addr;

    PingRequest request = {};
    AppStatus status = protocol_service_.decodePingRequest(frame, request);
    if (status != AppStatus::kOk) {
        result.status = status;
        return result;
    }

    result.sequence_id = request.sequence_id;
    result.has_sequence_id = request.has_sequence_id;

    if (!zigbee_driver_.isNetworkReady()) {
        result.status = AppStatus::kNetworkNotReady;
        return result;
    }

    PingResponsePayload response = {};
    status = protocol_service_.encodePingResponse(request, config_service_.getConfig(), response);
    if (status != AppStatus::kOk) {
        result.status = status;
        return result;
    }

    esp_err_t err = zigbee_driver_.sendPingResponse(
        request.sender_short_addr,
        response.bytes,
        response.len
    );
    result.driver_error = err;
    result.status = err == ESP_OK ? AppStatus::kOk : AppStatus::kDriverError;
    return result;
}
