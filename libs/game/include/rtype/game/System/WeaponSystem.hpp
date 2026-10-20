#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/GameContext.hpp"

namespace rtype::game
{

/// Runs the weapon (Armed) of each ship: waits the fire interval between two shots, spends one round per
/// shot and reloads on its own when the magazine is empty. While Fire is held and the weapon is ready, it
/// fires its projectile (spawnWeaponShot()) from the nose of the ship. The shots are created after the
/// systems of the tick, so they move from the next one.
class WeaponSystem : public ecs::System
{
    ecs::Ecs &_ecs;
    GameContext &_context;
    ecs::ComponentCache<Armed> &_weapons;
    ecs::ComponentCache<PlayerShip> &_ships;
    ecs::ComponentCache<ecs::Position> &_positions;

public:
    WeaponSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context);

    void update(float elapsed) override;
};

} // namespace rtype::game
