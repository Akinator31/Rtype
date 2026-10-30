#pragma once

#include "rtype/game/Enemy/EnemyBehavior.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace rtype::game
{

/// Everything that makes a kind of enemy: creating a new one is filling this and registering it in an
/// EnemyRegistry; loadGameConfig() reads them from config/enemies/ (ADR 0018). Speeds are in game units per
/// second.
struct EnemyDefinition {
    /// Unique name in a registry, e.g. "bydo_scout".
    std::string id;
    /// Its ship (model and sounds): the id of a ShipDefinition of the ShipRegistry.
    std::string ship;
    /// Hit points it starts with (not 0), the same unit as the Health component.
    std::uint8_t health = 1;
    /// Top speed (above 0).
    float speed = 300.0f;
    /// Speed gained per second until it reaches its top speed, in game units per second squared; 0 gives it
    /// its top speed at once.
    float acceleration = 600.0f;
    /// The id of a WeaponDefinition of the WeaponRegistry.
    std::string weapon;
    /// Points given to the player who destroys it.
    std::uint32_t score = 100;
    /// Hit points a player ship loses when it runs into it; the enemy keeps going.
    std::uint8_t collisionDamage = 20;
    /// What moves it and makes it fire; without one, it speeds up to the left and fires as soon as it is in
    /// the game area.
    std::optional<EnemyBehavior> behavior = std::nullopt;
    /// How many times its health is filled (not 0): once it runs out, it gets all of it back and starts its
    /// next phase, until the last one. A boss lasts longer than the 255 hit points of a Health.
    std::uint8_t phases = 1;
    /// The clients draw it as a boss (EntityKind::BydoBoss) instead of a flyer.
    bool boss = false;
};

} // namespace rtype::game
