#include "rtype/ecs/Hierarchy.hpp"

#include "rtype/ecs/Ecs.hpp"

namespace rtype::ecs
{

namespace
{

// Deeper hierarchies are a bug (most likely a loop): positions stop being computed there
constexpr int maxDepth = 64;

} // namespace

void attach(Ecs &ecs, entity_t child, entity_t parent, LocalPosition position)
{
    ecs.addComponent<Parent>(child, ecs.ref(parent));
    ecs.addComponent<LocalPosition>(child, position);
    if (!ecs.hasComponent<Position>(child)) {
        ecs.addComponent<Position>(child);
    }
}

bool isHidden(Ecs &ecs, entity_t entity)
{
    ComponentCache<Hidden> &hidden = ecs.cache<Hidden>();
    ComponentCache<Parent> &parents = ecs.cache<Parent>();
    for (int depth = 0; depth < maxDepth; ++depth) {
        if (hidden.has(entity)) {
            return true;
        }
        if (!parents.has(entity) || !ecs.isAlive(parents.get(entity).entity)) {
            return false;
        }
        entity = parents.get(entity).entity.id;
    }
    return false;
}

TransformSystem::TransformSystem(ComponentCaches &caches, Ecs &ecs)
    : _ecs(ecs), _parents(use<Parent>(caches)), _locals(use<LocalPosition>(caches)),
      _positions(use<Position>(caches))
{
}

Position TransformSystem::worldPosition(entity_t entity, int depth)
{
    const Position own = _positions.has(entity) ? _positions.get(entity) : Position{};
    if (depth >= maxDepth || !_parents.has(entity) || !_locals.has(entity)) {
        return own;
    }
    const EntityRef parent = _parents.get(entity).entity;
    if (!_ecs.isAlive(parent)) {
        return own;
    }
    const Position base = worldPosition(parent.id, depth + 1);
    const LocalPosition &local = _locals.get(entity);
    return {base.x + local.x, base.y + local.y};
}

void TransformSystem::update(float /*elapsed*/)
{
    // Computing from the root every time keeps the result right whatever the order of the entities; the
    // hierarchies of the game are shallow, so the cost stays small
    for (entity_t entity = 0; entity < _parents.size(); ++entity) {
        if (_parents.has(entity) && _locals.has(entity) && _positions.has(entity)) {
            _positions.get(entity) = worldPosition(entity, 0);
        }
    }
}

} // namespace rtype::ecs
