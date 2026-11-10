#pragma once

#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/game/Projectile/ProjectileCategory.hpp"
#include "rtype/game/Snapshot.hpp"
#include "rtype/physics/Component/Motion.hpp"

#include <cstdint>
#include <optional>
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
    /// The velocity it was fired with: the category moves it from this one (ProjectileSystem).
    physics::Velocity launch;
    /// Ticks since it was fired.
    std::uint32_t age = 0;
    /// Distance after which it vanishes, 0 for no limit (ProjectileSystem).
    float range = 0.0f;
    /// Distance flown since it was fired.
    float travelled = 0.0f;
};

/// A ship with a weapon, from the WeaponRegistry of the GameContext: its state, run by the WeaponSystem.
struct Armed {
    /// Id of the WeaponDefinition.
    std::string weapon;
    /// Ticks before it can fire again.
    std::uint32_t cooldown = 0;
    /// Shots left in the magazine (unused when the magazine is infinite).
    std::uint32_t ammo = 0;
    /// Ticks before the magazine is full again, 0 when it is not reloading.
    std::uint32_t reloadLeft = 0;
};

/// Hit points left.
struct Health {
    std::uint8_t points = 0;
};

/// The ship of a player, controlled by its input.
struct PlayerShip {
    std::uint8_t slot = 0;
};

/// The ship of the configuration the entity flies, sent to the clients so that they draw its model: its index
/// in the ShipRegistry (EntityState::ship).
struct ShipIndex {
    std::uint8_t value = NoShip;
};

/// A Bydo, made from its EnemyDefinition (spawnEnemy()).
struct Enemy {
    /// Id of the EnemyDefinition.
    std::string id;
    /// Ticks since it appeared.
    std::uint32_t age = 0;
    /// Its phase, from 1 to EnemyDefinition::phases: each one starts with all its health.
    std::uint8_t phase = 1;
};

/// A stance a Bydo takes for a tick, ordered by its behavior, which the clients animate
/// (EntityState::Guarding, Slashing, Shooting).
enum class EnemyStance : std::uint8_t {
    None,
    /// Behind its guard: the shots of the players that hit it do no damage.
    Guard,
    /// Striking with a blade, usually while it charges.
    Slash,
    /// Aiming its gun.
    Shoot,
};

/// What the behavior of a Bydo orders it, if its EnemyDefinition has one (EnemySystem): it then fires only
/// when ordered to (WeaponSystem).
struct EnemyOrders {
    /// Where it flies at its top speed, to stop there; none, it keeps its velocity.
    std::optional<ecs::Position> destination;
    /// Fire at this tick, if its weapon is ready: straight ahead, or towards `target`.
    bool fire = false;
    std::optional<ecs::Position> target;
    /// Its stance at this tick, ordered again at every tick.
    EnemyStance stance = EnemyStance::None;
    /// The shots of the players do no damage at this tick, whatever its stance, ordered again at every tick.
    bool invulnerable = false;
};

/// Hurts what it hits: the shots of a player hurt the Bydos, the Bydos and their shots hurt the players.
struct Damage {
    /// Hit points it takes from what it hits.
    std::uint8_t points = 0;
};

/// Cannot be hit for some ticks: a ship that just appeared.
struct Invulnerable {
    std::uint32_t ticks = 0;
};

/// An ally that comes to the rescue against the final boss (RescueSystem): flown and fired by the rescue,
/// never hit.
struct Ally {
    /// Its place in the formation, 0 to 4.
    std::uint8_t index = 0;
    /// Ticks it is still thrown back by a dash of the shogun.
    std::uint32_t stunned = 0;
};

/// Removed (DestroyCause::Removed) once entirely outside the game area, by more than Rules::areaMargin:
/// shots that flew past.
struct LeavesArea {
    /// Half the size of the entity, so that it is removed once out of sight.
    float halfWidth = 0.0f;
    float halfHeight = 0.0f;
};

} // namespace rtype::game
