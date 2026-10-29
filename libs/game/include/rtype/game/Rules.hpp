#pragma once

#include <cstdint>

namespace rtype::game
{

/// The numbers of the gameplay, in one place to tune them: speeds in game units per second, durations in
/// ticks (TickRate per second). A Simulation is made with its rules, so that the tests can use their own.
struct Rules {
    /// Lives of each player at the start.
    std::uint8_t lives = 3;

    /// Speed of a ship, in every direction (diagonals included).
    float shipSpeed = 600.0f;
    /// Half the size of a ship: it stays entirely in the game area. The proportions of the hull of its model,
    /// as the client draws it.
    float shipHalfWidth = 84.0f;
    float shipHalfHeight = 49.0f;

    /// A missile speeds up by this fraction of its launch speed per tick, up to missileMaxBoost times it.
    float missileBoostPerTick = 0.02f;
    float missileMaxBoost = 3.0f;
    /// A plasma projectile goes up then down at this vertical speed, switching every plasmaZigzagInterval
    /// ticks.
    float plasmaZigzagSpeed = 200.0f;
    std::uint32_t plasmaZigzagInterval = 20;

    /// Ticks between the loss of a ship and the next one, if the player has lives left.
    std::uint32_t respawnDelay = 180;
    /// Ticks a new ship cannot be hit.
    std::uint32_t invulnerableTicks = 120;
    /// Ticks a ship that was hit, and survived, cannot be hit again.
    std::uint32_t hitInvulnerableTicks = 60;

    /// Ticks before the first wave, once a player is in the game.
    std::uint32_t firstWaveDelay = 120;
    /// Ticks between the end of a wave and the start of the next one.
    std::uint32_t wavePause = 180;

    /// Seed of the random numbers of a game: the same seed and the same inputs give the same game.
    std::uint32_t seed = 1;

    /// How far outside the game area an entity that leaves it is removed, in game units.
    float areaMargin = 128.0f;
};

} // namespace rtype::game
