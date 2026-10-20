#pragma once

#include "rtype/game/Projectile/ProjectileCategory.hpp"
#include "rtype/game/Snapshot.hpp"

#include <cstdint>
#include <string>

/// The components of the game. The position, the velocity and the collider are those of the ECS and of the
/// physics (rtype::ecs::Position, rtype::physics::Velocity, rtype::physics::Collider): the position is the
/// centre of the entity.
namespace rtype::game
{

/// The id of an entity in the snapshots, never reused within a game. Every entity the clients see has one,
/// with a Kind: the ids of the ECS are reused, these are not.
struct NetworkId {
    std::uint32_t value = 0;
};

/// What the entity is, for the clients.
struct Kind {
    EntityKind value = EntityKind::PlayerShip;
};

/// The player the entity belongs to: its ship, its shots.
struct Owner {
    std::uint8_t slot = 0;
};

/// A projectile, as made from its ProjectileDefinition: what the client needs to draw it.
struct Projectile {
    ProjectileCategory category = ProjectileCategory::Bullet;
    /// Path of the 3D model.
    std::string model;
};

/// Hit points left.
struct Health {
    std::uint8_t points = 0;
};

/// The ship of a player, controlled by its input.
struct PlayerShip {
    std::uint8_t slot = 0;
    /// Ticks before it can fire again.
    std::uint32_t fireCooldown = 0;
};

/// Cannot be hit for some ticks: a ship that just appeared.
struct Invulnerable {
    std::uint32_t ticks = 0;
};

/// Removed (DestroyCause::Removed) once entirely outside the game area, by more than Rules::areaMargin:
/// shots that flew past.
struct LeavesArea {
    /// Half the size of the entity, so that it is removed once out of sight.
    float halfWidth = 0.0f;
    float halfHeight = 0.0f;
};

} // namespace rtype::game
