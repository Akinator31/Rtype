#pragma once

#include <cstdint>

namespace rtype::ecs
{

/// An entity is only an id: it is the index of its components in every ComponentCache.
using entity_t = std::uint32_t;

} // namespace rtype::ecs
