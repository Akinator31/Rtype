#pragma once

#include "ecs/Component/ComponentCaches.hpp"
#include "ecs/Entity/Entity.hpp"
#include "ecs/System/System.hpp"

#include <memory>
#include <utility>
#include <vector>

namespace ecs {

class Ecs {
    std::vector<bool> _alive;
    std::vector<entity_t> _freeIds;
    std::vector<std::unique_ptr<System>> _systems;
    ComponentCaches _componentCaches;

  public:
    /// Creates an entity, reusing the id of a destroyed one if there is any.
    [[nodiscard]] entity_t createEntity();

    /// Destroys an entity and its components. Does nothing if it is not alive.
    void destroyEntity(entity_t entity);

    [[nodiscard]] bool isAlive(entity_t entity) const;

    template <typename T, typename... Args> T& addComponent(entity_t entity, Args&&... args) {
        return _componentCaches.cache<T>().emplace(entity, std::forward<Args>(args)...);
    }

    template <typename T> void removeComponent(entity_t entity) {
        _componentCaches.cache<T>().erase(entity);
    }

    template <typename T> [[nodiscard]] bool hasComponent(entity_t entity) {
        return _componentCaches.cache<T>().has(entity);
    }

    /// The entity must have the component (see hasComponent()).
    template <typename T> [[nodiscard]] T& getComponent(entity_t entity) {
        return _componentCaches.cache<T>().get(entity);
    }

    /// Builds a system S(ComponentCaches&, args...) and returns it. The Ecs owns it.
    template <typename S, typename... Args> S& registerSystem(Args&&... args) {
        auto system = std::make_unique<S>(_componentCaches, std::forward<Args>(args)...);
        S& ref = *system;
        _systems.push_back(std::move(system));
        return ref;
    }

    /// Updates the enabled systems, in registration order.
    void update();
};

} // namespace ecs
