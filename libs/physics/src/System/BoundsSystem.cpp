#include "rtype/physics/System/BoundsSystem.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace rtype::physics
{

namespace
{

struct Footprint {
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float halfX = 0.0f;
    float halfY = 0.0f;
};

Footprint footprint(const Collider &collider)
{
    return std::visit([&](const auto &shape) -> Footprint {
        using T = std::decay_t<decltype(shape)>;
        if constexpr (std::is_same_v<T, Aabb>) {
            return {collider.offset.x + shape.center.x, collider.offset.y + shape.center.y,
                    std::max(0.0f, shape.halfSize.x), std::max(0.0f, shape.halfSize.y)};
        } else {
            const float radius = std::max(0.0f, shape.radius);
            return {collider.offset.x + shape.center.x, collider.offset.y + shape.center.y,
                    radius, radius};
        }
    }, collider.shape);
}

bool validZone(ecs::Position min, ecs::Position max)
{
    return std::isfinite(min.x) && std::isfinite(min.y) && std::isfinite(max.x) &&
           std::isfinite(max.y) && min.x <= max.x && min.y <= max.y;
}

int hierarchyDepth(ecs::Ecs &ecs, ecs::ComponentCache<ecs::Parent> &parents,
                   ecs::entity_t entity)
{
    int depth = 0;
    while (depth < 64 && parents.has(entity)) {
        const ecs::EntityRef parent = parents.get(entity).entity;
        if (!ecs.isAlive(parent)) {
            break;
        }
        entity = parent.id;
        ++depth;
    }
    return depth;
}

float clampCenter(float center, float offset, float half, float min, float max)
{
    const float lower = min + half - offset;
    const float upper = max - half - offset;
    if (lower > upper) {
        return (min + max) * 0.5f - offset;
    }
    return std::clamp(center, lower, upper);
}

bool blocked(float center, float offset, float half, float min, float max, float velocity)
{
    const float lower = min + half - offset;
    const float upper = max - half - offset;
    if (lower > upper) {
        return true;
    }
    return (center <= lower && velocity < 0.0f) || (center >= upper && velocity > 0.0f);
}

}

BoundsSystem::BoundsSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs,
                           std::function<bool(ecs::entity_t)> shouldUpdate)
    : _ecs(ecs), _positions(use<ecs::Position>(caches)),
      _localPositions(use<ecs::LocalPosition>(caches)), _parents(use<ecs::Parent>(caches)),
      _colliders(use<Collider>(caches)), _velocities(use<Velocity>(caches)),
      _statics(use<Static>(caches)), _confined(use<ConfinedToBounds>(caches)),
      _destroyOutOfBounds(use<DestroyOutOfBounds>(caches)),
      _shouldUpdate(std::move(shouldUpdate))
{
}

ecs::Position BoundsSystem::worldPosition(ecs::entity_t entity, int depth) const
{
    const ecs::Position own = _positions.has(entity) ? _positions.get(entity) : ecs::Position{};
    if (depth >= 64 || !_parents.has(entity) || !_localPositions.has(entity)) {
        return own;
    }
    const ecs::EntityRef parent = _parents.get(entity).entity;
    if (!_ecs.isAlive(parent)) {
        return own;
    }
    const ecs::Position base = worldPosition(parent.id, depth + 1);
    const ecs::LocalPosition &local = _localPositions.get(entity);
    return {base.x + local.x, base.y + local.y};
}

void BoundsSystem::setWorldPosition(ecs::entity_t entity, ecs::Position before,
                                    ecs::Position after)
{
    _positions.get(entity) = after;
    if (_parents.has(entity) && _localPositions.has(entity)) {
        ecs::LocalPosition &local = _localPositions.get(entity);
        local.x += after.x - before.x;
        local.y += after.y - before.y;
    }
}

void BoundsSystem::update(float)
{
    std::vector<std::pair<int, ecs::entity_t>> confined;
    for (ecs::entity_t entity = 0; entity < _confined.size(); ++entity) {
        if (_confined.has(entity) && _positions.has(entity) && _ecs.isAlive(entity) &&
            !_statics.has(entity) && (!_shouldUpdate || _shouldUpdate(entity))) {
            confined.emplace_back(hierarchyDepth(_ecs, _parents, entity), entity);
        }
    }
    std::sort(confined.begin(), confined.end());

    for (const auto &[depth, entity] : confined) {
        const ConfinedToBounds &zone = _confined.get(entity);
        if (!validZone(zone.min, zone.max)) {
            throw std::invalid_argument("ConfinedToBounds requires finite ordered limits");
        }
        const Footprint size = _colliders.has(entity) ? footprint(_colliders.get(entity)) : Footprint{};
        const ecs::Position before = worldPosition(entity);
        const ecs::Position after{
            clampCenter(before.x, size.offsetX, size.halfX, zone.min.x, zone.max.x),
            clampCenter(before.y, size.offsetY, size.halfY, zone.min.y, zone.max.y)};
        setWorldPosition(entity, before, after);

        if (_velocities.has(entity)) {
            Velocity &velocity = _velocities.get(entity);
            if (blocked(after.x, size.offsetX, size.halfX, zone.min.x, zone.max.x,
                        velocity.x)) {
                velocity.x = 0.0f;
            }
            if (blocked(after.y, size.offsetY, size.halfY, zone.min.y, zone.max.y,
                        velocity.y)) {
                velocity.y = 0.0f;
            }
        }
    }

    for (ecs::entity_t entity = 0; entity < _destroyOutOfBounds.size(); ++entity) {
        if (!_destroyOutOfBounds.has(entity) || !_positions.has(entity) || !_ecs.isAlive(entity) ||
            (_shouldUpdate && !_shouldUpdate(entity))) {
            continue;
        }
        const DestroyOutOfBounds &zone = _destroyOutOfBounds.get(entity);
        if (!validZone(zone.min, zone.max) || !std::isfinite(zone.margin) ||
            zone.margin < 0.0f) {
            throw std::invalid_argument("DestroyOutOfBounds requires valid limits and margin");
        }
        const Footprint size = _colliders.has(entity) ? footprint(_colliders.get(entity)) : Footprint{};
        const ecs::Position position = worldPosition(entity);
        const float centerX = position.x + size.offsetX;
        const float centerY = position.y + size.offsetY;
        if (centerX + size.halfX < zone.min.x - zone.margin ||
            centerX - size.halfX > zone.max.x + zone.margin ||
            centerY + size.halfY < zone.min.y - zone.margin ||
            centerY - size.halfY > zone.max.y + zone.margin) {
            const ecs::EntityRef reference = _ecs.ref(entity);
            _ecs.defer([ecs = &_ecs, reference] {
                if (ecs->isAlive(reference)) {
                    ecs->destroyEntity(reference.id);
                }
            });
        }
    }
}

}
