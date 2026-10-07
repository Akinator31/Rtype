#pragma once

#include "rtype/ecs/Entity/Entity.hpp"

namespace rtype::ecs
{

/// Type-erased base of ComponentCache, so ComponentCaches can drop an entity from every cache.
class ICache
{
public:
    virtual ~ICache();
    virtual void erase(entity_t entity) = 0;
};

} // namespace rtype::ecs
