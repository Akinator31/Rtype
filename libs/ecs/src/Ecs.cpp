#include "rtype/ecs/Ecs.hpp"

#include "rtype/ecs/Hierarchy.hpp"

namespace rtype::ecs
{

entity_t Ecs::createEntity()
{
    if (!_freeIds.empty()) {
        const entity_t entity = _freeIds.back();
        _freeIds.pop_back();
        _alive[entity] = true;
        return entity;
    }
    _alive.push_back(true);
    _generations.push_back(0);
    return static_cast<entity_t>(_alive.size() - 1);
}

void Ecs::destroyEntity(entity_t entity)
{
    if (!isAlive(entity)) {
        return;
    }
    // Children first, so that they never outlive their parent with a dangling reference
    const EntityRef self = ref(entity);
    ComponentCache<Parent> &parents = _componentCaches.cache<Parent>();
    std::vector<entity_t> children;
    for (entity_t child = 0; child < parents.size(); ++child) {
        if (parents.has(child) && parents.get(child).entity == self) {
            children.push_back(child);
        }
    }
    for (const entity_t child : children) {
        destroyEntity(child);
    }

    _componentCaches.erase(entity);
    _alive[entity] = false;
    // References to this entity become dead, even once its id is given to a new entity
    ++_generations[entity];
    _freeIds.push_back(entity);
}

bool Ecs::isAlive(entity_t entity) const
{
    return entity < _alive.size() && _alive[entity];
}

EntityRef Ecs::ref(entity_t entity) const
{
    return {entity, entity < _generations.size() ? _generations[entity] : 0};
}

bool Ecs::isAlive(EntityRef entity) const
{
    return isAlive(entity.id) && _generations[entity.id] == entity.generation;
}

void Ecs::defer(std::function<void()> action)
{
    _deferred.push_back(std::move(action));
}

void Ecs::update(float elapsed)
{
    for (auto &system : _systems) {
        if (system->enabled) {
            system->update(elapsed);
        }
    }
    // Deferred actions may defer others: run them until there are none left
    while (!_deferred.empty()) {
        std::vector<std::function<void()>> actions = std::move(_deferred);
        _deferred.clear();
        for (const auto &action : actions) {
            action();
        }
    }
}

} // namespace rtype::ecs
