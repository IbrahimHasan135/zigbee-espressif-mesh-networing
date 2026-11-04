#pragma once

#include <stdint.h>

static constexpr uint16_t CUSTOM_NAV_CLUSTER_ID = 0xFF01;
static constexpr uint8_t CMD_PING_REQ = 0x01;
static constexpr uint8_t CMD_PING_RSP = 0x02;

static constexpr uint8_t PING_REQ_SEQUENCE_LEN = 2;
static constexpr uint8_t PING_RSP_LEGACY_LEN = 4;
static constexpr uint8_t PING_RSP_FULL_LEN = 8;
