#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rtype::game
{

/// An item a wave makes appear later (WaveDefinition::items).
struct ScheduledItem {
    /// Ticks after the start of the wave.
    std::uint32_t tick = 0;
    /// The id of its ItemDefinition.
    std::string item;
};

/// Where a game is in its waves, run by the WaveSystem: wave n is a wave of difficulty n, from
/// MinWaveDifficulty to MaxWaveDifficulty, or any wave in an endless game (Rules::endless).
struct WaveProgress {
    /// The number of the last wave started, which is its difficulty but in an endless game; 0 before the
    /// first one.
    std::uint16_t number = 0;
    /// The name of the wave being played, empty between two waves.
    std::string name;
    /// Enemies of the wave already sent.
    std::uint16_t spawned = 0;
    /// Ticks before its next enemy.
    std::uint32_t nextSpawnIn = 0;
    /// Music wave: ticks since it started.
    std::uint32_t elapsed = 0;
    /// The items of the wave not appeared yet, by increasing tick, and the ticks since it started.
    std::vector<ScheduledItem> items;
    std::uint32_t itemTicks = 0;
    /// Ticks before the next wave starts, between two waves.
    std::uint32_t pauseLeft = 0;
};

} // namespace rtype::game
