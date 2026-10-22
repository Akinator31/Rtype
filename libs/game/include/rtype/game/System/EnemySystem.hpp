#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/GameContext.hpp"
#include "rtype/physics/Component/Motion.hpp"

namespace rtype::game
{

/// Flies the Bydos: each one speeds up to the left, by the acceleration of its EnemyDefinition, up to its
/// top speed. A straight line for now; the patterns of ADR 0019 will replace it.
class EnemySystem : public ecs::System
{
    ecs::Ecs &_ecs;
    GameContext &_context;
    ecs::ComponentCache<Enemy> &_enemies;
    ecs::ComponentCache<physics::Velocity> &_velocities;

public:
    EnemySystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context);

    void update(float elapsed) override;
};

} // namespace rtype::game
