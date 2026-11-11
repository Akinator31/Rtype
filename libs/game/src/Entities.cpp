#include "rtype/game/Entities.hpp"

#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/Hitbox.hpp"
#include "rtype/physics/Component/Bounds.hpp"
#include "rtype/physics/Component/Collider.hpp"
#include "rtype/physics/Component/Motion.hpp"

#include <algorithm>
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

/// The index the snapshots send for a ship: NoShip beyond what a byte holds.
std::uint8_t findShipIndex(const ShipRegistry &ships, const std::string &ship)
{
    const std::size_t index = ships.getIndex(ship);
    return index < NoShip ? static_cast<std::uint8_t>(index) : NoShip;
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
    equipLoadout(ecs, context, ship, context.players[slot].loadout);
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
    ecs.addComponent<ShipIndex>(entity, findShipIndex(context.ships, enemy.ship));
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
    if (owner < MaxPlayers) {
        const std::size_t index = context.projectiles.getIndex(id);
        ecs.addComponent<ProjectileIndex>(projectile,
                                          index < NoShip ? static_cast<std::uint8_t>(index) : NoShip);
    }
    return projectile;
}

ecs::entity_t spawnItem(ecs::Ecs &ecs, GameContext &context, const std::string &itemId, float x, float y)
{
    const ItemDefinition &item = context.items.getDefinition(itemId);
    const std::size_t index = context.items.getIndex(itemId);
    const ecs::entity_t entity = spawnVisible(ecs, context, EntityKind::Item, NoOwner, x, y);
    const auto lifetime =
        static_cast<std::uint32_t>(std::lround(item.lifetime * static_cast<float>(TickRate)));
    ecs.addComponent<Item>(entity, itemId, 0U, std::max<std::uint32_t>(1, lifetime));
    ecs.addComponent<ItemIndex>(entity, index < NoShip ? static_cast<std::uint8_t>(index) : NoShip);
    ecs.addComponent<physics::Velocity>(entity, -item.speed, 0.0f);
    ecs.addComponent<physics::Collider>(entity, item.collider);
    const physics::Vec3 half = halfSizeOf(item.collider);
    ecs.addComponent<LeavesArea>(entity, half.x, half.y);
    return entity;
}

void equipWeapon(ecs::Ecs &ecs, GameContext &context, ecs::entity_t ship, const std::string &weaponId)
{
    const WeaponDefinition &weapon = context.weapons.getDefinition(weaponId);
    ecs.addComponent<Armed>(ship, weaponId, 0U, weapon.magazineSize, 0U);
}

void equipLoadout(ecs::Ecs &ecs, GameContext &context, ecs::entity_t ship, const Loadout &loadout)
{
    const std::string &first = loadout[0].empty() ? context.player.weapon : loadout[0];
    if (!first.empty()) {
        equipWeapon(ecs, context, ship, first);
    }
    if (first.empty() || loadout[1].empty()) {
        if (ecs.hasComponent<Holster>(ship)) {
            ecs.removeComponent<Holster>(ship);
        }
        return;
    }
    const WeaponDefinition &second = context.weapons.getDefinition(loadout[1]);
    ecs.addComponent<Holster>(ship, Armed{loadout[1], 0U, second.magazineSize, 0U}, std::uint8_t{0});
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
