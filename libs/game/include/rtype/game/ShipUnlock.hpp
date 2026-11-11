#pragma once

#include <cmath>
#include <cstdint>
#include <string>

namespace rtype::game
{

/// The characteristics of a ship of the hangar, from 0 to 1: the client shows them as gauges, the game makes
/// them a factor of the ship of the player (statFactor()). 0.5 is the ship of the rules, as it was without
/// them.
struct ShipStats {
    /// Of Rules::shipSpeed.
    float speed = 0.5f;
    /// Of the damage of its shots.
    float power = 0.5f;
    /// Of the health of its ship (PlayerDefinition::health).
    float armor = 0.5f;

    friend bool operator==(const ShipStats &, const ShipStats &) = default;
};

/// The least and the most a characteristic of a ship multiplies by, at 0 and at 1.
inline constexpr float MinStatFactor = 0.75f;
inline constexpr float MaxStatFactor = 1.25f;

/// What a characteristic from 0 to 1 multiplies by: MinStatFactor at 0, 1 at 0.5, MaxStatFactor at 1. The
/// game and the prediction of the client use it, so that they move the ship the same, to the bit.
[[nodiscard]] constexpr float statFactor(float stat)
{
    return MinStatFactor + (MaxStatFactor - MinStatFactor) * stat;
}

/// A value of the rules scaled by a characteristic, rounded, from 1 to 255: the health of a ship, the damage
/// of a shot.
[[nodiscard]] inline std::uint8_t scaleByStat(std::uint8_t value, float stat)
{
    const long scaled = std::lround(static_cast<float>(value) * statFactor(stat));
    return static_cast<std::uint8_t>(scaled < 1 ? 1 : (scaled > 0xFF ? 0xFF : scaled));
}

/// A ship of the hangar of the client, the level of the account from which a player may fly it, and its
/// characteristics, read from config/hangar.lua. The ships of the hangar are those of the client, named by
/// their code ("VX-01"): the game only knows their codes and characteristics (Simulation::setShip()).
struct ShipUnlock {
    std::string ship;
    /// From 1: a ship of level 1 is open to every player.
    std::uint32_t level = 1;
    ShipStats stats;
};

} // namespace rtype::game
