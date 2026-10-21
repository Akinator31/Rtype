#pragma once

#include "rtype/game/Wave/WaveArea.hpp"
#include "rtype/game/Wave/WavePlacement.hpp"
#include "rtype/game/Wave/WaveSpawn.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace rtype::game
{

/// A group of enemies that appear one after the other. Creating a new wave is filling this and registering it
/// in a WaveRegistry (ADR 0018 will load it from a file).
struct WaveDefinition {
    /// Unique name in a registry, e.g. "scouts_line".
    std::string name;
    /// Short text shown to the players, e.g. "Scout line".
    std::string label;
    std::string description;
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
};

} // namespace rtype::game
