#pragma once

#include <cstdint>
#include <string>

namespace rtype::game
{

/// Everything that makes a kind of enemy: creating a new one is filling this and registering it in an
/// EnemyRegistry (ADR 0018 will load it from a file). Speeds are in game units per second.
struct EnemyDefinition {
    /// Unique name in a registry, e.g. "bydo_scout".
    std::string id;
    /// Its ship (model and sounds): the id of a ShipDefinition of the ShipRegistry.
    std::string ship;
    /// Hit points it starts with (not 0), the same unit as the Health component.
    std::uint8_t health = 1;
    /// Top speed (above 0).
    float speed = 300.0f;
    /// Speed gained per second until it reaches its top speed, in game units per second squared.
    float acceleration = 600.0f;
    /// The id of a WeaponDefinition of the WeaponRegistry.
    std::string weapon;
};

} // namespace rtype::game
