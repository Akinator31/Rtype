#pragma once

#include "ecs/Component/ComponentCache.hpp"
#include "ecs/Component/ComponentCaches.hpp"

#include <typeindex>
#include <vector>

namespace ecs
{

class System
{
    std::vector<std::type_index> _componentTypes;

protected:
    /// Declares that the system uses component T and returns its cache. To call once per type,
    /// from the constructor of the system, which then keeps the reference.
    template <typename T>
    ComponentCache<T> &use(ComponentCaches &caches)
    {
        _componentTypes.emplace_back(typeid(T));
        return caches.cache<T>();
    }

public:
    bool enabled = true;

    virtual ~System();
    virtual void update() = 0;

    /// Component types used by the system.
    [[nodiscard]] const std::vector<std::type_index> &componentTypes() const;
};

} // namespace ecs
