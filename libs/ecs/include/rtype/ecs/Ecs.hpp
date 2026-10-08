#pragma once

#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Entity/Entity.hpp"
#include "rtype/ecs/System/System.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace rtype::ecs
{

class Ecs
{
    std::vector<bool> _alive;
    std::vector<std::uint32_t> _generations;
    std::vector<entity_t> _freeIds;
    std::vector<std::unique_ptr<System>> _systems;
    std::vector<std::function<void()>> _deferred;
    ComponentCaches _componentCaches;

public:
    /// Creates an entity, reusing the id of a destroyed one if there is any.
    [[nodiscard]] entity_t createEntity();

    /// Destroys an entity and its components, and its children (entities whose Parent is this one),
    /// recursively. Does nothing if it is not alive.
    void destroyEntity(entity_t entity);

    [[nodiscard]] bool isAlive(entity_t entity) const;

    /// A reference to an entity, to keep it (see EntityRef).
    [[nodiscard]] EntityRef ref(entity_t entity) const;
    /// Tells whether the referenced entity still exists: false once it is destroyed, even if its id was
    /// reused.
    [[nodiscard]] bool isAlive(EntityRef entity) const;

    template <typename T, typename... Args>
    T &addComponent(entity_t entity, Args &&...args)
    {
        return _componentCaches.cache<T>().emplace(entity, std::forward<Args>(args)...);
    }

    template <typename T>
    void removeComponent(entity_t entity)
    {
        _componentCaches.cache<T>().erase(entity);
    }

    template <typename T>
    [[nodiscard]] bool hasComponent(entity_t entity)
    {
        return _componentCaches.cache<T>().has(entity);
    }

    /// The entity must have the component (see hasComponent()).
    template <typename T>
    [[nodiscard]] T &getComponent(entity_t entity)
    {
        return _componentCaches.cache<T>().get(entity);
    }

    /// The cache of a component type, to go through every entity that has it.
    template <typename T>
    [[nodiscard]] ComponentCache<T> &cache()
    {
        return _componentCaches.cache<T>();
    }

    /// Builds a system S(ComponentCaches&, args...) and returns it. The Ecs owns it.
    template <typename S, typename... Args>
    S &registerSystem(Args &&...args)
    {
        auto system = std::make_unique<S>(_componentCaches, std::forward<Args>(args)...);
        S &ref = *system;
        _systems.push_back(std::move(system));
        return ref;
    }

    /// Runs an action after the systems of the current update, or after those of the next one if called
    /// outside of an update. Use it to change the world while a system goes through it: a button that opens a
    /// new screen would otherwise create entities, and reallocate the caches, while the system handling
    /// buttons is still reading them.
    void defer(std::function<void()> action);

    /// Updates the enabled systems, in registration order, then runs the deferred actions.
    /// @param elapsed Time since the previous update, in seconds, given to every system.
    void update(float elapsed);
};

} // namespace rtype::ecs
