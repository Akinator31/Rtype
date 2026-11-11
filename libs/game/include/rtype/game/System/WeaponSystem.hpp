#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/GameContext.hpp"
#include "rtype/physics/Component/Collider.hpp"

namespace rtype::game
{

/// Runs the weapon (Armed) of each ship: waits the fire interval between two shots, spends one round per
/// shot and reloads on its own when the magazine is empty. A ready weapon fires its projectile
/// (spawnWeaponShot()) from the nose of the ship: a player's while Fire is held, to the right, or turned up
/// or down by Rules::aimAngle while AimUp or AimDown is held (spawnAimedShot()); a Bydo's as soon as it is in
/// the game area, to the left, or, if it has EnemyOrders, when they say so, to the left or towards their
/// target (spawnAimedShot()). The shots are created after the systems of the tick, so they move from the next
/// one. A player with two weapons first takes in hand the one its input asks for (selectWeapon()).
class WeaponSystem : public ecs::System
{
    ecs::Ecs &_ecs;
    GameContext &_context;
    ecs::ComponentCache<Armed> &_weapons;
    ecs::ComponentCache<Holster> &_holsters;
    ecs::ComponentCache<PlayerShip> &_ships;
    ecs::ComponentCache<Enemy> &_enemies;
    ecs::ComponentCache<EnemyOrders> &_orders;
    ecs::ComponentCache<ecs::Position> &_positions;
    ecs::ComponentCache<physics::Collider> &_colliders;

    [[nodiscard]] bool wantsToFire(ecs::entity_t entity) const;
    void fire(ecs::entity_t entity, const std::string &weapon);

public:
    WeaponSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context);

    void update(float elapsed) override;
};

} // namespace rtype::game
