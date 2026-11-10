#pragma once

#include "rtype/physics/Component/Collider.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace rtype::game
{

/// What an item does to the ship of the player who picks it up. The behaviors are written in C++
/// (applyItemEffect()); the files of config/items/ only choose them and set their numbers.
enum class ItemEffectType : std::uint8_t {
    /// Gives back `amount` hit points, up to the health of the player (PlayerDefinition::health).
    Heal,
    /// Gives `amount` lives more to the player, up to 255.
    ExtraLife,
    /// Takes `amount` hit points from the ship: a trap. A ship left with none is lost, as when it is shot.
    /// Nothing happens to a ship that cannot be hit (Invulnerable).
    Damage,
    /// Fires `factor` times faster for `duration` seconds: the fire interval of its weapon is divided by
    /// `factor`, never below one tick. A new one starts the duration again.
    RapidFire,
    /// Cannot be hit for `duration` seconds (Invulnerable), or longer if it already could not.
    Shield,
};

/// One behavior of an item, with its numbers: those its type does not use stay at their defaults.
struct ItemEffect {
    ItemEffectType type = ItemEffectType::Heal;
    /// Heal, Damage: hit points. ExtraLife: lives.
    std::uint8_t amount = 0;
    /// RapidFire, Shield: seconds.
    float duration = 0.0f;
    /// RapidFire: how many times faster, above 1.
    float factor = 1.0f;
};

/// Everything that makes a kind of item: creating a new one is filling this and registering it in an
/// ItemRegistry; loadGameConfig() reads them from config/items/ (ADR 0018). The waves make them appear at
/// random places of the screen (WaveDefinition::items); a ship of a player that touches one picks it up and
/// gets its effects, in order.
struct ItemDefinition {
    /// Unique name in a registry, e.g. "repair_kit".
    std::string id;
    /// The 3D model the client draws, relative to its assets, e.g. "models/items/repair_kit.glb".
    std::string model;
    /// Its hitbox: the ships that touch it pick it up. The client draws the model at its size.
    physics::Collider collider;
    /// What it does, in order: at least one.
    std::vector<ItemEffect> effects;
    /// Seconds it stays before it vanishes, if nobody picks it up (above 0).
    float lifetime = 10.0f;
    /// Speed it drifts to the left at, in game units per second (0 or more): it goes with the scrolling.
    float speed = 60.0f;
};

} // namespace rtype::game
