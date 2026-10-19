#pragma once

#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ICache.hpp"
#include "rtype/ecs/Entity/Entity.hpp"

#include <memory>
#include <typeindex>
#include <unordered_map>

namespace rtype::ecs
{

/// One ComponentCache per component type.
/// The lookup by type is done once (systems keep the reference it returns), never per entity.
class ComponentCaches
{
    std::unordered_map<std::type_index, std::unique_ptr<ICache>> _caches;

public:
    /// The cache of T, created on first use. The reference stays valid as long as this object lives.
    template <typename T>
    ComponentCache<T> &cache()
    {
        auto &slot = _caches[std::type_index(typeid(T))];
        if (!slot) {
            slot = std::make_unique<ComponentCache<T>>();
        }
        return static_cast<ComponentCache<T> &>(*slot);
    }

    /// Removes every component of an entity.
    void erase(entity_t entity);
};

} // namespace rtype::ecs
