#pragma once

#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/physics/Component/Collider.hpp"
#include "rtype/physics/Component/CollisionFilter.hpp"
#include "rtype/physics/Component/Motion.hpp"
#include "rtype/physics/Component/Static.hpp"
#include "rtype/physics/Component/FastMoving.hpp"
#include "rtype/physics/Geometry/Shapes.hpp"
#include "rtype/physics/System/FrameSystem.hpp"

#include <cstdint>
#include <functional>
#include <unordered_set>
#include <vector>

namespace rtype::physics
{

struct Collision {
    ecs::EntityRef a;
    ecs::EntityRef b;
    Vec3 normal;
    float depth = 0.0f;
};

class CollisionSystem : public ecs::System, public FrameSystem
{
    ecs::Ecs &_ecs;
    ecs::ComponentCache<ecs::Position> &_positions;
    ecs::ComponentCache<Collider> &_colliders;
    ecs::ComponentCache<CollisionLayer> &_layers;
    ecs::ComponentCache<CollisionMask> &_masks;
    ecs::ComponentCache<Static> &_statics;
    ecs::ComponentCache<FastMoving> &_fastMoving;
    ecs::ComponentCache<Velocity> &_velocities;
    ecs::ComponentCache<ecs::Parent> &_parents;
    ecs::ComponentCache<ecs::LocalPosition> &_localPositions;
    float _cellSize;
    std::function<bool(ecs::entity_t)> _shouldUpdate;
    std::vector<Collision> _collisions;
    std::unordered_set<std::uint64_t> _seenPairs;
    bool _frameActive = false;

    void clearCollisions();
    void resolve(ecs::entity_t first, ecs::entity_t second);

public:
    CollisionSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, float cellSize = 64.0f,
                    std::function<bool(ecs::entity_t)> shouldUpdate = {});

    void beginFrame() override;
    void endFrame() override;
    void update(float elapsed) override;

    [[nodiscard]] const std::vector<Collision> &collisions() const;
};

}
