#pragma once

#include <string>

namespace rtype::game
{

/// One enemy of a Manual wave: its kind and where it appears, in game units.
struct WaveSpawn {
    /// The id of an EnemyDefinition of the EnemyRegistry.
    std::string enemy;
    float x = 0.0f;
    float y = 0.0f;
};

} // namespace rtype::game
