#pragma once

#include "esp_err.h"

#include "common/nav_types.h"
#include "drivers/zigbee/zigbee_router_driver.hpp"
#include "services/config/router_config_service.hpp"
#include "services/navigation_protocol/navigation_protocol_service.hpp"

class RouterPointService {
public:
    RouterPointService(ZigbeeRouterDriver &zigbee_driver,
                       RouterConfigService &config_service,
                       NavigationProtocolService &protocol_service);

    RouterPointProcessResult handleIncomingFrame(const ZigbeeFrame &frame);

private:
    ZigbeeRouterDriver &zigbee_driver_;
    RouterConfigService &config_service_;
    NavigationProtocolService &protocol_service_;
};
