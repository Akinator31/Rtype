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

struct SweptHit {
    Vec3 normal;
    Vec3 away;
    float time;
    ecs::entity_t mobile;
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

Vec3 displacement(ecs::entity_t entity, float elapsed,
                  const ecs::ComponentCache<Velocity> &velocities,
                  const ecs::ComponentCache<Static> &statics)
{
    if (elapsed <= 0.0f || statics.has(entity) || !velocities.has(entity)) {
        return {};
    }
    const Velocity &velocity = velocities.get(entity);
    return {velocity.x * elapsed, velocity.y * elapsed, 0.0f};
}

Aabb sweptBounds(Aabb box, Vec3 movement)
{
    box.center.x -= movement.x * 0.5f;
    box.center.y -= movement.y * 0.5f;
    box.halfSize.x += std::abs(movement.x) * 0.5f;
    box.halfSize.y += std::abs(movement.y) * 0.5f;
    return box;
}

std::optional<RayHit> sweepAabb(const Aabb &moving, Vec3 movement, const Aabb &target)
{
    if (movement.x == 0.0f && movement.y == 0.0f) {
        return std::nullopt;
    }
    Aabb expanded = target;
    expanded.halfSize.x += std::max(0.0f, moving.halfSize.x);
    expanded.halfSize.y += std::max(0.0f, moving.halfSize.y);
    expanded.halfSize.z += std::max(0.0f, moving.halfSize.z);
    const auto hit = raycast(Ray{moving.center, movement}, expanded);
    if (!hit || hit->t > 1.0f || hit->t < 0.0f) {
        return std::nullopt;
    }
    if (hit->t == 0.0f && hit->normal.x == 0.0f && hit->normal.y == 0.0f &&
        hit->normal.z == 0.0f) {
        return std::nullopt;
    }
    return hit;
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
      _statics(use<Static>(caches)), _fastMoving(use<FastMoving>(caches)),
      _velocities(use<Velocity>(caches)),
      _parents(use<ecs::Parent>(caches)), _localPositions(use<ecs::LocalPosition>(caches)),
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

void CollisionSystem::update(float elapsed)
{
    if (!_frameActive) {
        clearCollisions();
    }

    std::vector<Entry> entries;
    std::vector<Aabb> bounds;
    bool hasFastMover = false;
    for (ecs::entity_t entity = 0; entity < _fastMoving.size(); ++entity) {
        if (_fastMoving.has(entity) && _colliders.has(entity) && _positions.has(entity) &&
            _ecs.isAlive(entity) && (!_shouldUpdate || _shouldUpdate(entity))) {
            hasFastMover = true;
            break;
        }
    }
    for (ecs::entity_t entity = 0; entity < _colliders.size(); ++entity) {
        if (!_colliders.has(entity) || !_positions.has(entity) || !_ecs.isAlive(entity) ||
            (_shouldUpdate && !_shouldUpdate(entity))) {
            continue;
        }
        Shape shape = worldShape(_colliders.get(entity), _positions.get(entity));
        Aabb box = boundsOf(shape);
        if (hasFastMover && std::holds_alternative<Aabb>(shape)) {
            box = sweptBounds(box, displacement(entity, elapsed, _velocities, _statics));
        }
        bounds.push_back(box);
        entries.push_back({entity, std::move(shape)});
    }

    const BroadPhaseResult candidates = broadPhaseGrid(bounds, _cellSize);
    std::vector<std::pair<ecs::entity_t, ecs::entity_t>> solidPairs;
    std::vector<SweptHit> sweptSolids;
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
        std::optional<SweptHit> swept;
        if (!overlap && std::holds_alternative<Aabb>(a.shape) &&
            std::holds_alternative<Aabb>(b.shape) &&
            (_fastMoving.has(a.entity) || _fastMoving.has(b.entity))) {
            const bool firstMoves = _fastMoving.has(a.entity);
            const Entry &moving = firstMoves ? a : b;
            const Entry &target = firstMoves ? b : a;
            const Vec3 movingDisplacement = displacement(moving.entity, elapsed, _velocities,
                                                         _statics);
            const Vec3 targetDisplacement = displacement(target.entity, elapsed, _velocities,
                                                         _statics);
            const Vec3 relative{movingDisplacement.x - targetDisplacement.x,
                                movingDisplacement.y - targetDisplacement.y, 0.0f};
            Aabb movingStart = std::get<Aabb>(moving.shape);
            Aabb targetStart = std::get<Aabb>(target.shape);
            movingStart.center.x -= movingDisplacement.x;
            movingStart.center.y -= movingDisplacement.y;
            targetStart.center.x -= targetDisplacement.x;
            targetStart.center.y -= targetDisplacement.y;
            if (const auto hit = sweepAabb(movingStart, relative, targetStart)) {
                const float sign = firstMoves ? -1.0f : 1.0f;
                swept = SweptHit{{hit->normal.x * sign, hit->normal.y * sign,
                                  hit->normal.z * sign}, hit->normal, hit->t, moving.entity};
            }
        }
        if (!overlap && !swept) {
            continue;
        }
        if (_seenPairs.insert(pairKey(a.entity, b.entity)).second) {
            _collisions.push_back({_ecs.ref(a.entity), _ecs.ref(b.entity),
                                   overlap ? overlap->normal : swept->normal,
                                   overlap ? overlap->depth : 0.0f});
        }
        if (!_colliders.get(a.entity).trigger && !_colliders.get(b.entity).trigger &&
            (_statics.has(a.entity) != _statics.has(b.entity))) {
            if (overlap) {
                solidPairs.emplace_back(a.entity, b.entity);
            } else if (swept && !_statics.has(swept->mobile)) {
                sweptSolids.push_back(*swept);
            }
        }
    }

    for (const auto &[first, second] : solidPairs) {
        resolve(first, second);
    }
    std::sort(sweptSolids.begin(), sweptSolids.end(), [](const SweptHit &a, const SweptHit &b) {
        return a.time < b.time;
    });
    std::unordered_set<ecs::entity_t> stopped;
    for (const SweptHit &hit : sweptSolids) {
        if (!stopped.insert(hit.mobile).second) {
            continue;
        }
        const Vec3 movement = displacement(hit.mobile, elapsed, _velocities, _statics);
        ecs::Position &position = _positions.get(hit.mobile);
        const float x = movement.x * (hit.time - 1.0f);
        const float y = movement.y * (hit.time - 1.0f);
        position.x += x;
        position.y += y;
        if (_parents.has(hit.mobile) && _localPositions.has(hit.mobile)) {
            ecs::LocalPosition &local = _localPositions.get(hit.mobile);
            local.x += x;
            local.y += y;
        }
        if (_velocities.has(hit.mobile)) {
            Velocity &velocity = _velocities.get(hit.mobile);
            const float inward = velocity.x * hit.away.x + velocity.y * hit.away.y;
            if (inward < 0.0f) {
                velocity.x -= inward * hit.away.x;
                velocity.y -= inward * hit.away.y;
            }
        }
    }
}

void CollisionSystem::resolve(ecs::entity_t first, ecs::entity_t second)
{
    const bool firstStatic = _statics.has(first);
    const ecs::entity_t mobile = firstStatic ? second : first;
    const Shape firstShape = worldShape(_colliders.get(first), _positions.get(first));
    const Shape secondShape = worldShape(_colliders.get(second), _positions.get(second));
    const auto overlap = std::visit([](const auto &a, const auto &b) {
        return penetration(a, b);
    }, firstShape, secondShape);
    if (!overlap || overlap->depth <= 0.0f) {
        return;
    }

    const float direction = firstStatic ? 1.0f : -1.0f;
    const float x = overlap->normal.x * direction;
    const float y = overlap->normal.y * direction;
    if (x == 0.0f && y == 0.0f) {
        return;
    }

    ecs::Position &position = _positions.get(mobile);
    position.x += x * overlap->depth;
    position.y += y * overlap->depth;
    if (_parents.has(mobile) && _localPositions.has(mobile)) {
        ecs::LocalPosition &local = _localPositions.get(mobile);
        local.x += x * overlap->depth;
        local.y += y * overlap->depth;
    }
    if (_velocities.has(mobile)) {
        Velocity &velocity = _velocities.get(mobile);
        const float inward = velocity.x * x + velocity.y * y;
        if (inward < 0.0f) {
            velocity.x -= inward * x;
            velocity.y -= inward * y;
        }
    }
}

const std::vector<Collision> &CollisionSystem::collisions() const
{
    return _collisions;
}

}
