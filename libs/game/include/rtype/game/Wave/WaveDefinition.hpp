#pragma once

#include "rtype/game/Wave/WaveArea.hpp"
#include "rtype/game/Wave/WaveBeatRule.hpp"
#include "rtype/game/Wave/WavePlacement.hpp"
#include "rtype/game/Wave/WaveSpawn.hpp"
#include "rtype/game/Wave/WaveTimedSpawn.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace rtype::game
{

/// Items a wave makes appear: how many of one kind, at random places of the screen and at random times while
/// its enemies come.
struct WaveItem {
    /// The id of an ItemDefinition.
    std::string item;
    /// How many appear: `quantity` (above 0), or a number drawn from `quantity` to `maxQuantity` each time
    /// the wave is played, when `maxQuantity` is above it.
    std::uint16_t quantity = 1;
    std::uint16_t maxQuantity = 0;
};

/// The difficulty of the easiest waves, and of the hardest: the last one, the final boss.
constexpr std::uint8_t MinWaveDifficulty = 1;
constexpr std::uint8_t MaxWaveDifficulty = 6;

/// A group of enemies that appear one after the other. Creating a new wave is filling this and registering it
/// in a WaveRegistry; loadGameConfig() reads them from config/waves/ (ADR 0018).
struct WaveDefinition {
    /// Unique name in a registry, e.g. "scouts_line".
    std::string name;
    /// Short text shown to the players, e.g. "Scout line".
    std::string label;
    std::string description;
    /// How hard the wave is, from MinWaveDifficulty to MaxWaveDifficulty.
    std::uint8_t difficulty = MinWaveDifficulty;
    /// How many enemies the wave has (above 0). For a Manual wave, it is the size of `spawns`.
    std::uint16_t quantity = 1;
    /// Seconds between two enemies (0 or more; 0 makes them all appear at once).
    float spawnInterval = 1.0f;
    WavePlacement placement = WavePlacement::Random;
    /// Manual only: each enemy and its position, in the order they appear.
    std::vector<WaveSpawn> spawns;
    /// Random only: the id of an EnemyDefinition, the kind of every enemy of the wave.
    std::string enemy;
    /// Random only: where they appear.
    WaveArea area;
    /// The music the clients play when the wave starts: a path relative to the assets of the client, for
    /// instance "sounds/boss_theme.mp3". Empty: the music playing goes on. A Music wave needs it, and follows
    /// it.
    std::string music;
    /// Music only: what comes at the beats of each band, in the order bass, mid, high.
    std::array<WaveBeatRule, 3> beats;
    /// Music only: seconds an enemy needs to cross the screen, so that it comes in sight before its beat,
    /// not on it.
    float lead = 1.5f;
    /// The items that appear during the wave, whatever its placement.
    std::vector<WaveItem> items;
    /// Music only: the enemies and the tick they appear at, by increasing tick. Not written in the files:
    /// loadGameConfig() fills it from the music, and `quantity` with its size.
    std::vector<WaveTimedSpawn> timeline;
};

} // namespace rtype::game
