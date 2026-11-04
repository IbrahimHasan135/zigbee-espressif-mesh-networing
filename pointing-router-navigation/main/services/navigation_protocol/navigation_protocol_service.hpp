#pragma once

#include "common/nav_types.h"

class NavigationProtocolService {
public:
    AppStatus decodePingRequest(const ZigbeeFrame &frame, PingRequest &out_request) const;
    AppStatus encodePingResponse(const PingRequest &request,
                                 const RouterPointConfig &config,
                                 PingResponsePayload &out_payload) const;
};
