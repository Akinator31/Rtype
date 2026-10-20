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

physics::Vec3 halfSizeOf(const physics::Collider &collider)
{
    if (const auto *box = std::get_if<physics::Aabb>(&collider.shape)) {
        return box->halfSize;
    }
    const float radius = std::get<physics::Sphere>(collider.shape).radius;
    return {radius, radius, radius};
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

ecs::entity_t spawnProjectile(ecs::Ecs &ecs, GameContext &context, const std::string &id, std::uint8_t owner,
                              float x, float y, float velocityX, float velocityY)
{
    const ProjectileDefinition &definition = context.projectiles.getDefinition(id);
    // The clients draw only the kinds they know: a projectile is a player shot or a Bydo shot for now
    const EntityKind kind = owner < MaxPlayers ? EntityKind::PlayerShot : EntityKind::BydoShot;
    const ecs::entity_t projectile = spawnVisible(ecs, context, kind, owner, x, y);
    ecs.addComponent<Projectile>(projectile, definition.category, definition.model,
                                 physics::Velocity{velocityX, velocityY}, 0U);
    ecs.addComponent<physics::Velocity>(projectile, velocityX, velocityY);
    ecs.addComponent<physics::Collider>(projectile, definition.collider);
    const physics::Vec3 half = halfSizeOf(definition.collider);
    ecs.addComponent<LeavesArea>(projectile, half.x, half.y);
    return projectile;
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
