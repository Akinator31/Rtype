#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/physics/Component/Bounds.hpp"
#include "rtype/physics/Component/Collider.hpp"
#include "rtype/physics/Component/Motion.hpp"
#include "rtype/physics/Component/Static.hpp"

#include <functional>

namespace rtype::physics
{

class BoundsSystem : public ecs::System
{
    ecs::Ecs &_ecs;
    ecs::ComponentCache<ecs::Position> &_positions;
    ecs::ComponentCache<ecs::LocalPosition> &_localPositions;
    ecs::ComponentCache<ecs::Parent> &_parents;
    ecs::ComponentCache<Collider> &_colliders;
    ecs::ComponentCache<Velocity> &_velocities;
    ecs::ComponentCache<Static> &_statics;
    ecs::ComponentCache<ConfinedToBounds> &_confined;
    ecs::ComponentCache<DestroyOutOfBounds> &_destroyOutOfBounds;
    std::function<bool(ecs::entity_t)> _shouldUpdate;

    [[nodiscard]] ecs::Position worldPosition(ecs::entity_t entity, int depth = 0) const;
    void setWorldPosition(ecs::entity_t entity, ecs::Position before, ecs::Position after);

public:
    BoundsSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs,
                 std::function<bool(ecs::entity_t)> shouldUpdate = {});

    void update(float elapsed) override;
};

} // namespace rtype::physics
