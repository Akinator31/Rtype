#include "ecs/Ecs.hpp"

namespace ecs
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
    return static_cast<entity_t>(_alive.size() - 1);
}

void Ecs::destroyEntity(entity_t entity)
{
    if (!isAlive(entity)) {
        return;
    }
    _componentCaches.erase(entity);
    _alive[entity] = false;
    _freeIds.push_back(entity);
}

bool Ecs::isAlive(entity_t entity) const
{
    return entity < _alive.size() && _alive[entity];
}

void Ecs::update()
{
    for (auto &system : _systems) {
        if (system->enabled) {
            system->update();
        }
    }
}

} // namespace ecs
