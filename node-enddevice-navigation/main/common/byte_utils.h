#pragma once

#include <stdint.h>

inline uint16_t readU16Be(const uint8_t *data)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
}

inline void writeU16Be(uint8_t *data, uint16_t value)
{
    data[0] = static_cast<uint8_t>((value >> 8) & 0xFFU);
    data[1] = static_cast<uint8_t>(value & 0xFFU);
}

