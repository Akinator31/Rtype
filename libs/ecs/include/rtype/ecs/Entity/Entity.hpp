#pragma once

#include <cstdint>

namespace rtype::ecs
{

/// An entity is only an id: it is the index of its components in every ComponentCache.
using entity_t = std::uint32_t;

/// A reference to an entity that can be kept for a long time, for instance by a child to its parent.
///
/// Entity ids are reused once an entity is destroyed: a plain entity_t kept somewhere could end up
/// designating another entity. The generation counts how many times the id was reused, so a reference to a
/// destroyed entity is recognised as dead (Ecs::isAlive()) even after its id was given to a new entity.
struct EntityRef {
    entity_t id = 0;
    std::uint32_t generation = 0;

    friend bool operator==(const EntityRef &, const EntityRef &) = default;
};

} // namespace rtype::ecs
