#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/GameContext.hpp"
#include "rtype/physics/Component/Motion.hpp"

namespace rtype::game
{

/// Turns the input of each player into the movement of its ship, and makes it shoot while Fire is held, every
/// Rules::fireInterval ticks. The shots are created after the systems of the tick, so they move from the next
/// one. A ship with a weapon (Armed) is left to the WeaponSystem.
class PlayerControlSystem : public ecs::System
{
    ecs::Ecs &_ecs;
    GameContext &_context;
    ecs::ComponentCache<PlayerShip> &_ships;
    ecs::ComponentCache<Armed> &_weapons;
    ecs::ComponentCache<physics::Velocity> &_velocities;
    ecs::ComponentCache<ecs::Position> &_positions;

public:
    PlayerControlSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context);

    void update(float elapsed) override;
};

} // namespace rtype::game
