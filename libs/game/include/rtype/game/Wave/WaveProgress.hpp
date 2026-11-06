#pragma once

#include <cstdint>
#include <string>

namespace rtype::game
{

/// Where a game is in its waves, run by the WaveSystem: wave n is a wave of difficulty n, from
/// MinWaveDifficulty to MaxWaveDifficulty.
struct WaveProgress {
    /// The number of the last wave started, which is its difficulty; 0 before the first one.
    std::uint8_t number = 0;
    /// The name of the wave being played, empty between two waves.
    std::string name;
    /// Enemies of the wave already sent.
    std::uint16_t spawned = 0;
    /// Ticks before its next enemy.
    std::uint32_t nextSpawnIn = 0;
    /// Music wave: ticks since it started.
    std::uint32_t elapsed = 0;
    /// Ticks before the next wave starts, between two waves.
    std::uint32_t pauseLeft = 0;
};

} // namespace rtype::game
