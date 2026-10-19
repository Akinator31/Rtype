#include "rtype/game/Entities.hpp"

#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/physics/Component/Bounds.hpp"
#include "rtype/physics/Component/Collider.hpp"
#include "rtype/physics/Component/Motion.hpp"

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
    // The four slots share the height of the game area evenly
    const float y = GameHeight * static_cast<float>(slot + 1) / static_cast<float>(MaxPlayers + 1);
    const ecs::entity_t ship = spawnVisible(ecs, context, EntityKind::PlayerShip, slot, ShipStartX, y);
    ecs.addComponent<PlayerShip>(ship, slot, 0U);
    ecs.addComponent<Health>(ship, rules.shipHealth);
    ecs.addComponent<physics::Velocity>(ship);
    ecs.addComponent<physics::Collider>(ship, box(rules.shipHalfWidth, rules.shipHalfHeight));
    ecs.addComponent<physics::ConfinedToBounds>(ship, ecs::Position{0.0f, 0.0f},
                                                ecs::Position{GameWidth, GameHeight});
    return ship;
}

ecs::entity_t spawnPlayerShot(ecs::Ecs &ecs, GameContext &context, std::uint8_t slot, float x, float y)
{
    const Rules &rules = context.rules;
    const ecs::entity_t shot = spawnVisible(ecs, context, EntityKind::PlayerShot, slot, x, y);
    ecs.addComponent<physics::Velocity>(shot, rules.shotSpeed, 0.0f);
    ecs.addComponent<physics::Collider>(shot, box(rules.shotHalfWidth, rules.shotHalfHeight));
    ecs.addComponent<LeavesArea>(shot, rules.shotHalfWidth, rules.shotHalfHeight);
    return shot;
}

ecs::entity_t spawnEnemy(ecs::Ecs &ecs, GameContext &context)
{
    const Rules &rules = context.rules;
    // Entirely inside the area vertically, just outside on the right: it comes in
    const auto minY = static_cast<std::uint32_t>(rules.enemyHalfHeight);
    const auto maxY = static_cast<std::uint32_t>(GameHeight - rules.enemyHalfHeight);
    const float y = static_cast<float>(context.randomBetween(minY, maxY));
    const ecs::entity_t enemy =
        spawnVisible(ecs, context, EntityKind::BydoShooter, NoOwner, GameWidth + rules.enemyHalfWidth, y);
    ecs.addComponent<Enemy>(enemy, context.randomBetween(rules.enemyMinFireInterval, rules.enemyMaxFireInterval));
    ecs.addComponent<Health>(enemy, rules.enemyHealth);
    ecs.addComponent<physics::Velocity>(enemy, -rules.enemySpeed, 0.0f);
    ecs.addComponent<physics::Collider>(enemy, box(rules.enemyHalfWidth, rules.enemyHalfHeight));
    ecs.addComponent<LeavesArea>(enemy, rules.enemyHalfWidth, rules.enemyHalfHeight);
    return enemy;
}

ecs::entity_t spawnEnemyShot(ecs::Ecs &ecs, GameContext &context, float x, float y)
{
    const Rules &rules = context.rules;
    const ecs::entity_t shot = spawnVisible(ecs, context, EntityKind::BydoShot, NoOwner, x, y);
    ecs.addComponent<physics::Velocity>(shot, -rules.enemyShotSpeed, 0.0f);
    ecs.addComponent<physics::Collider>(shot, box(rules.enemyShotHalfWidth, rules.enemyShotHalfHeight));
    ecs.addComponent<LeavesArea>(shot, rules.enemyShotHalfWidth, rules.enemyShotHalfHeight);
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
