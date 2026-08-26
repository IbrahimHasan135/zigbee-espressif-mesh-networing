#pragma once

#include <stdint.h>

static inline uint16_t readU16Be(const uint8_t *bytes)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8) | bytes[1]);
}

static inline void writeU16Be(uint8_t *bytes, uint16_t value)
{
    bytes[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
    bytes[1] = static_cast<uint8_t>(value & 0xFF);
}
