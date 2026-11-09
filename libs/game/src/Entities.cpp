#include "rtype/game/Entities.hpp"

#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/Hitbox.hpp"
#include "rtype/physics/Component/Bounds.hpp"
#include "rtype/physics/Component/Collider.hpp"
#include "rtype/physics/Component/Motion.hpp"

#include <cmath>

namespace rtype::game
{

namespace
{

/// Distance of the ships from the left edge when they appear.
constexpr float ShipStartX = 200.0f;

physics::Collider box(float halfWidth, float halfHeight)
{
    return {physics::Aabb{{}, {halfWidth, halfHeight, 0.0f}}, {}, false};
}

/// What the clients see of a projectile: a shot of a player, or a plasma orb or another shot of a Bydo.
EntityKind findProjectileKind(std::uint8_t owner, ProjectileCategory category)
{
    if (owner < MaxPlayers) {
        return EntityKind::PlayerShot;
    }
    return category == ProjectileCategory::Plasma ? EntityKind::BydoPlasmaShot : EntityKind::BydoShot;
}

ecs::entity_t spawnVisible(ecs::Ecs &ecs, GameContext &context, EntityKind kind, std::uint8_t owner, float x,
                           float y)
{
    const ecs::entity_t entity = ecs.createEntity();
    ecs.addComponent<NetworkId>(entity, context.newId());
    ecs.addComponent<Kind>(entity, kind);
    ecs.addComponent<Owner>(entity, owner);
    ecs.addComponent<ecs::Position>(entity, x, y);
    return entity;
}

} // namespace

ecs::entity_t spawnPlayerShip(ecs::Ecs &ecs, GameContext &context, std::uint8_t slot)
{
    const Rules &rules = context.rules;
    const float y = GameHeight * static_cast<float>(slot + 1) / static_cast<float>(MaxPlayers + 1);
    const ecs::entity_t ship = spawnVisible(ecs, context, EntityKind::PlayerShip, slot, ShipStartX, y);
    ecs.addComponent<PlayerShip>(ship, slot);
    ecs.addComponent<Health>(ship, context.player.health);
    ecs.addComponent<physics::Velocity>(ship);
    ecs.addComponent<physics::Collider>(ship, box(rules.shipHalfWidth, rules.shipHalfHeight));
    ecs.addComponent<physics::ConfinedToBounds>(ship, ecs::Position{0.0f, 0.0f},
                                                ecs::Position{GameWidth, GameHeight});
    if (!context.player.weapon.empty()) {
        equipWeapon(ecs, context, ship, context.player.weapon);
    }
    return ship;
}

ecs::entity_t spawnAlly(ecs::Ecs &ecs, GameContext &context, std::uint8_t index, float x, float y)
{
    const ecs::entity_t ally = spawnVisible(ecs, context, EntityKind::AllyShip, NoOwner, x, y);
    ecs.addComponent<Ally>(ally, index, 0U);
    ecs.addComponent<physics::Velocity>(ally);
    return ally;
}

ecs::entity_t spawnEnemy(ecs::Ecs &ecs, GameContext &context, const std::string &enemyId, float x, float y)
{
    const EnemyDefinition &enemy = context.enemies.getDefinition(enemyId);
    const physics::Collider &collider = context.ships.getDefinition(enemy.ship).collider;
    static_cast<void>(context.weapons.getDefinition(enemy.weapon));
    const EntityKind kind = enemy.boss ? EntityKind::BydoBoss : EntityKind::BydoFlyer;
    const ecs::entity_t entity = spawnVisible(ecs, context, kind, NoOwner, x, y);
    ecs.addComponent<Enemy>(entity, enemyId);
    ecs.addComponent<Health>(entity, enemy.health);
    ecs.addComponent<Damage>(entity, enemy.collisionDamage);
    ecs.addComponent<physics::Velocity>(entity);
    ecs.addComponent<physics::Collider>(entity, collider);
    const physics::Vec3 half = halfSizeOf(collider);
    ecs.addComponent<LeavesArea>(entity, half.x, half.y);
    if (enemy.behavior) {
        ecs.addComponent<EnemyOrders>(entity);
    }
    equipWeapon(ecs, context, entity, enemy.weapon);
    return entity;
}

ecs::entity_t spawnProjectile(ecs::Ecs &ecs, GameContext &context, const std::string &id, std::uint8_t owner,
                              float x, float y, float velocityX, float velocityY)
{
    const ProjectileDefinition &definition = context.projectiles.getDefinition(id);
    const EntityKind kind = findProjectileKind(owner, definition.category);
    const ecs::entity_t projectile = spawnVisible(ecs, context, kind, owner, x, y);
    ecs.addComponent<Projectile>(projectile, definition.category, definition.model,
                                 physics::Velocity{velocityX, velocityY}, 0U);
    ecs.addComponent<physics::Velocity>(projectile, velocityX, velocityY);
    ecs.addComponent<physics::Collider>(projectile, definition.collider);
    const physics::Vec3 half = halfSizeOf(definition.collider);
    ecs.addComponent<LeavesArea>(projectile, half.x, half.y);
    return projectile;
}

void equipWeapon(ecs::Ecs &ecs, GameContext &context, ecs::entity_t ship, const std::string &weaponId)
{
    const WeaponDefinition &weapon = context.weapons.getDefinition(weaponId);
    ecs.addComponent<Armed>(ship, weaponId, 0U, weapon.magazineSize, 0U);
}

ecs::entity_t spawnWeaponShot(ecs::Ecs &ecs, GameContext &context, const std::string &weaponId,
                              std::uint8_t owner, float x, float y)
{
    const float forward = owner < MaxPlayers ? 1.0f : -1.0f;
    return spawnAimedShot(ecs, context, weaponId, owner, x, y, x + forward, y);
}

ecs::entity_t spawnAimedShot(ecs::Ecs &ecs, GameContext &context, const std::string &weaponId,
                             std::uint8_t owner, float x, float y, float targetX, float targetY)
{
    const WeaponDefinition &weapon = context.weapons.getDefinition(weaponId);
    const float halfWidth = halfSizeOf(context.projectiles.getDefinition(weapon.projectile).collider).x;
    return spawnShot(ecs, context, weaponId, owner,
                     aimShot(weapon, halfWidth, owner, {x, y}, {targetX, targetY}));
}

ecs::entity_t spawnShot(ecs::Ecs &ecs, GameContext &context, const std::string &weaponId, std::uint8_t owner,
                        const ShotLaunch &launch)
{
    const WeaponDefinition &weapon = context.weapons.getDefinition(weaponId);
    const ecs::entity_t shot = spawnProjectile(ecs, context, weapon.projectile, owner, launch.position.x,
                                               launch.position.y, launch.velocity.x, launch.velocity.y);
    Projectile &projectile = ecs.getComponent<Projectile>(shot);
    projectile.category = weapon.mechanic;
    projectile.range = weapon.range;
    ecs.addComponent<Damage>(shot, weapon.damage);
    return shot;
}

std::optional<ecs::entity_t> findPlayerShip(ecs::Ecs &ecs, std::uint8_t slot)
{
    ecs::ComponentCache<PlayerShip> &ships = ecs.cache<PlayerShip>();
    for (ecs::entity_t entity = 0; entity < ships.size(); ++entity) {
        if (ships.has(entity) && ecs.isAlive(entity) && ships.get(entity).slot == slot) {
            return entity;
        }
    }
    return std::nullopt;
}

void destroyEntity(ecs::Ecs &ecs, GameContext &context, ecs::entity_t entity, DestroyCause cause)
{
    if (!ecs.isAlive(entity)) {
        return;
    }
    if (ecs.hasComponent<NetworkId>(entity)) {
        context.destroyed.push_back({ecs.getComponent<NetworkId>(entity).value, cause});
    }
    ecs.destroyEntity(entity);
}

} // namespace rtype::game
