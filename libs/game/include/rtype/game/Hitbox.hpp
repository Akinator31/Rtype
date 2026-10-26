#pragma once

#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/physics/Component/Collider.hpp"

/// The shapes of the entities, where they are: what the game needs to know what touches what.
namespace rtype::game
{

/// Half the size of the box around a collider: a sphere gives its radius on every axis.
[[nodiscard]] physics::Vec3 halfSizeOf(const physics::Collider &collider);

/// Tells whether two colliders touch, each one around the position of its entity (and its offset).
[[nodiscard]] bool overlaps(const ecs::Position &first, const physics::Collider &firstCollider,
                            const ecs::Position &second, const physics::Collider &secondCollider);

} // namespace rtype::game
