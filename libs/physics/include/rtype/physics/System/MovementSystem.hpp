#pragma once

#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/physics/Component/Motion.hpp"

#include <functional>

namespace rtype::physics
{

class MovementSystem : public ecs::System
{
    ecs::ComponentCache<Velocity> &_velocities;
    ecs::ComponentCache<Acceleration> &_accelerations;
    ecs::ComponentCache<MaxSpeed> &_maxSpeeds;
    ecs::ComponentCache<ecs::Position> &_positions;
    ecs::ComponentCache<ecs::LocalPosition> &_localPositions;
    ecs::ComponentCache<ecs::Parent> &_parents;
    std::function<bool(ecs::entity_t)> _shouldUpdate;

public:
    explicit MovementSystem(ecs::ComponentCaches &caches,
                            std::function<bool(ecs::entity_t)> shouldUpdate = {});

    void update(float elapsed) override;
};

} // namespace rtype::physics
