#pragma once

#include <cstdint>

namespace rtype::game
{

/// How the enemies of a wave get their position. One mode per wave: a wave does not mix the two.
enum class WavePlacement : std::uint8_t {
    /// The wave lists each enemy and where it appears (WaveDefinition::spawns).
    Manual,
    /// The wave gives one kind of enemy, placed at random in WaveDefinition::area.
    Random,
};

} // namespace rtype::game
