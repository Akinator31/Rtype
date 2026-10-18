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
    /// Half the size of a ship: it stays entirely in the game area.
    float shipHalfWidth = 48.0f;
    float shipHalfHeight = 24.0f;
    /// Hit points of a ship.
    std::uint8_t shipHealth = 1;

    /// Speed of a shot, to the right.
    float shotSpeed = 1500.0f;
    /// Half the size of a shot.
    float shotHalfWidth = 12.0f;
    float shotHalfHeight = 4.0f;
    /// Ticks between two shots while Fire is held: 10 is 6 shots per second.
    std::uint32_t fireInterval = 10;

    /// How far outside the game area an entity that leaves it is removed, in game units.
    float areaMargin = 128.0f;
};

} // namespace rtype::game
