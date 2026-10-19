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

    /// Ticks between two Bydos appearing, while there are fewer than maxEnemies.
    std::uint32_t enemySpawnInterval = 90;
    /// Most Bydos alive at the same time.
    std::uint32_t maxEnemies = 5;
    /// Speed of a Bydo, to the left.
    float enemySpeed = 150.0f;
    /// Half the size of a Bydo.
    float enemyHalfWidth = 40.0f;
    float enemyHalfHeight = 28.0f;
    /// Hit points of a Bydo.
    std::uint8_t enemyHealth = 1;
    /// A Bydo shoots after a random number of ticks between these two, 60 to 180 is every 1 to 3 seconds.
    std::uint32_t enemyMinFireInterval = 60;
    std::uint32_t enemyMaxFireInterval = 180;
    /// Speed of a shot of a Bydo, to the left.
    float enemyShotSpeed = 500.0f;
    /// Half the size of a shot of a Bydo.
    float enemyShotHalfWidth = 10.0f;
    float enemyShotHalfHeight = 10.0f;
    /// Points for the player whose shot destroys a Bydo.
    std::uint32_t enemyScore = 100;

    /// Ticks between the loss of a ship and the next one, if the player has lives left.
    std::uint32_t respawnDelay = 90;
    /// Ticks a new ship cannot be hit.
    std::uint32_t invulnerableTicks = 120;

    /// Seed of the random numbers of a game: the same seed and the same inputs give the same game.
    std::uint32_t seed = 1;

    /// How far outside the game area an entity that leaves it is removed, in game units.
    float areaMargin = 128.0f;
};

} // namespace rtype::game
