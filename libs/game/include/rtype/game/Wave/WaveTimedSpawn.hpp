#pragma once

#include "rtype/game/Wave/WaveSpawn.hpp"

#include <cstdint>

namespace rtype::game
{

/// One enemy of a Music wave: what appears, where, and the tick of the wave it appears at.
struct WaveTimedSpawn {
    std::uint32_t tick = 0;
    WaveSpawn spawn;
};

} // namespace rtype::game
