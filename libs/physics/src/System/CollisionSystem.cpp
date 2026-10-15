#include "rtype/physics/System/CollisionSystem.hpp"

#include "rtype/physics/Geometry/BroadPhase.hpp"
#include "rtype/physics/Geometry/Intersections.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace rtype::physics
{

namespace
{

using Shape = std::variant<Aabb, Sphere>;

struct Entry {
    ecs::entity_t entity;
    Shape shape;
};

Shape worldShape(const Collider &collider, const ecs::Position &position)
{
    Shape shape = collider.shape;
    std::visit([&](auto &value) {
        value.center.x += position.x + collider.offset.x;
        value.center.y += position.y + collider.offset.y;
        value.center.z += collider.offset.z;
    }, shape);
    return shape;
}

Aabb boundsOf(const Shape &shape)
{
    return std::visit([](const auto &value) -> Aabb {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, Aabb>) {
            return value;
        } else {
            const float radius = std::max(0.0f, value.radius);
            return {value.center, {radius, radius, radius}};
        }
    }, shape);
}

std::uint64_t pairKey(ecs::entity_t a, ecs::entity_t b)
{
    return (static_cast<std::uint64_t>(a) << 32) | b;
}

}

CollisionSystem::CollisionSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, float cellSize,
                                 std::function<bool(ecs::entity_t)> shouldUpdate)
    : _ecs(ecs), _positions(use<ecs::Position>(caches)), _colliders(use<Collider>(caches)),
      _layers(use<CollisionLayer>(caches)), _masks(use<CollisionMask>(caches)),
      _cellSize(cellSize), _shouldUpdate(std::move(shouldUpdate))
{
    if (!std::isfinite(cellSize) || cellSize <= 0.0f) {
        throw std::invalid_argument("CollisionSystem cell size must be positive and finite");
    }
}

void CollisionSystem::clearCollisions()
{
    _collisions.clear();
    _seenPairs.clear();
}

void CollisionSystem::beginFrame()
{
    clearCollisions();
    _frameActive = true;
}

void CollisionSystem::endFrame()
{
    _frameActive = false;
}

void CollisionSystem::update(float)
{
    if (!_frameActive) {
        clearCollisions();
    }

    std::vector<Entry> entries;
    std::vector<Aabb> bounds;
    for (ecs::entity_t entity = 0; entity < _colliders.size(); ++entity) {
        if (!_colliders.has(entity) || !_positions.has(entity) || !_ecs.isAlive(entity) ||
            (_shouldUpdate && !_shouldUpdate(entity))) {
            continue;
        }
        Shape shape = worldShape(_colliders.get(entity), _positions.get(entity));
        bounds.push_back(boundsOf(shape));
        entries.push_back({entity, std::move(shape)});
    }

    const BroadPhaseResult candidates = broadPhaseGrid(bounds, _cellSize);
    for (const BroadPhasePair pair : candidates.pairs) {
        const Entry &a = entries[pair.first];
        const Entry &b = entries[pair.second];
        const CollisionFilter aFilter{_layers.has(a.entity) ? &_layers.get(a.entity) : nullptr,
                                      _masks.has(a.entity) ? &_masks.get(a.entity) : nullptr};
        const CollisionFilter bFilter{_layers.has(b.entity) ? &_layers.get(b.entity) : nullptr,
                                      _masks.has(b.entity) ? &_masks.get(b.entity) : nullptr};
        if (!canCollide(aFilter, bFilter)) {
            continue;
        }

        const auto overlap = std::visit([](const auto &first, const auto &second) {
            return penetration(first, second);
        }, a.shape, b.shape);
        if (!overlap || !_seenPairs.insert(pairKey(a.entity, b.entity)).second) {
            continue;
        }
        _collisions.push_back({_ecs.ref(a.entity), _ecs.ref(b.entity), overlap->normal,
                               overlap->depth});
    }
}

const std::vector<Collision> &CollisionSystem::collisions() const
{
    return _collisions;
}

}
