#pragma once

#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"

#include <typeindex>
#include <vector>

namespace rtype::ecs
{

/// Logic run on the entities that have some components. A system gets its caches with use() in its
/// constructor, then goes through the entity ids in update(). Built and owned by Ecs::registerSystem().
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
    /// A disabled system is skipped by Ecs::update().
    bool enabled = true;

    virtual ~System();
    /// Runs the system once.
    /// @param elapsed Time since the previous update, in seconds: systems never read a clock themselves, so
    /// that tests and the server control time.
    virtual void update(float elapsed) = 0;

    /// Component types used by the system.
    [[nodiscard]] const std::vector<std::type_index> &componentTypes() const;
};

} // namespace rtype::ecs
