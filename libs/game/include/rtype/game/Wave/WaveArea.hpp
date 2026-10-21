#pragma once

#include "rtype/game/Constants.hpp"

namespace rtype::game
{

/// The rectangle where the enemies of a Random wave appear, in game units. By default a band just past the
/// right edge of the game area, over its whole height, so that they come in from the side.
struct WaveArea {
    float minX = GameWidth;
    float maxX = GameWidth + 100.0f;
    float minY = 0.0f;
    float maxY = GameHeight;
};

} // namespace rtype::game
