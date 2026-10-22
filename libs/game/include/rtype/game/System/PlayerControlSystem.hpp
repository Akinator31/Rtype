#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/GameContext.hpp"
#include "rtype/physics/Component/Motion.hpp"

namespace rtype::game
{

/// Turns the input of each player into the movement of its ship. Firing is the WeaponSystem's.
class PlayerControlSystem : public ecs::System
{
    ecs::Ecs &_ecs;
    GameContext &_context;
    ecs::ComponentCache<PlayerShip> &_ships;
    ecs::ComponentCache<physics::Velocity> &_velocities;

public:
    PlayerControlSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context);

    void update(float elapsed) override;
};

} // namespace rtype::game
