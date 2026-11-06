#pragma once

#include <cstdint>

namespace rtype::game
{

/// How the enemies of a wave get their time and their position. One mode per wave: a wave does not mix them.
enum class WavePlacement : std::uint8_t {
    /// The wave lists each enemy and where it appears (WaveDefinition::spawns).
    Manual,
    /// The wave gives one kind of enemy, placed at random in WaveDefinition::area.
    Random,
    /// The enemies come on the beats of a music (WaveDefinition::beats): loadGameConfig() analyses the music
    /// and fills WaveDefinition::timeline.
    Music,
};

} // namespace rtype::game
