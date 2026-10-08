#pragma once

#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Entity/Entity.hpp"
#include "rtype/ecs/System/System.hpp"

namespace rtype::ecs
{

class Ecs;

/// Position of an entity in the world, in game units. For an entity with a Parent, it is computed by the
/// TransformSystem from its LocalPosition: do not set it by hand.
struct Position {
    float x = 0.0f;
    float y = 0.0f;
};

/// Position of an entity relative to its Parent, in game units.
struct LocalPosition {
    float x = 0.0f;
    float y = 0.0f;
};

/// Attaches an entity to another one, its parent: the Force to the ship, a turret to a boss, a button to its
/// panel. The child moves with its parent (see TransformSystem), is hidden with it (see isHidden()), and is
/// destroyed with it (see Ecs::destroyEntity()).
struct Parent {
    EntityRef entity;
};

/// Hides an entity and its children, without destroying them: a closed dropdown keeps its options.
struct Hidden {};

/// Attaches `child` to `parent` at the given position relative to it.
void attach(Ecs &ecs, entity_t child, entity_t parent, LocalPosition position = {});

/// Tells whether an entity is hidden: it has Hidden, or one of its ancestors does.
[[nodiscard]] bool isHidden(Ecs &ecs, entity_t entity);

/// Computes the Position of every entity that has a Parent and a LocalPosition: the position of the parent
/// plus the local position. Parents are computed before their children, whatever their order.
///
/// Register it after the systems that move entities, and before the ones that use positions (collisions,
/// rendering).
class TransformSystem : public System
{
    Ecs &_ecs;
    ComponentCache<Parent> &_parents;
    ComponentCache<LocalPosition> &_locals;
    ComponentCache<Position> &_positions;

    // Position of an entity in the world, computing the ones of its ancestors if needed
    Position worldPosition(entity_t entity, int depth);

public:
    TransformSystem(ComponentCaches &caches, Ecs &ecs);

    void update(float elapsed) override;
};

} // namespace rtype::ecs
