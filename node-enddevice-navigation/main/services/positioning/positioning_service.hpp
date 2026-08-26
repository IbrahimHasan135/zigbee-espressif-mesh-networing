#pragma once

#include <stddef.h>

#include "common/nav_types.h"

class PositioningService {
public:
    float estimateDistanceCm(int8_t rssi) const;
    AppStatus calculatePosition(const NavAnchorSample *samples, size_t sample_count, Position2D &out_position) const;
};

