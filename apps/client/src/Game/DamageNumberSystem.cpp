#include "Game/DamageNumberSystem.hpp"

#include <algorithm>

namespace rtype::client
{

DamageNumberSystem::DamageNumberSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs)
    : _ecs(ecs), _numbers(use<DamageNumber>(caches)), _positions(use<ecs::Position>(caches)),
      _opacities(use<engine::Opacity>(caches))
{
}

void DamageNumberSystem::update(float elapsed)
{
    for (ecs::entity_t entity = 0; entity < _numbers.size(); ++entity) {
        if (!_numbers.has(entity)) {
            continue;
        }
        DamageNumber &number = _numbers.get(entity);
        number.time += elapsed;
        if (number.time >= number.duration) {
            _ecs.destroyEntity(entity);
            continue;
        }
        showNumber(entity, number, elapsed);
    }
}

void DamageNumberSystem::showNumber(ecs::entity_t entity, const DamageNumber &number, float elapsed)
{
    if (_positions.has(entity)) {
        // y points down in the game
        _positions.get(entity).y -= RiseSpeed * elapsed;
    }
    if (_opacities.has(entity)) {
        const float left = (number.duration - number.time) / (number.duration * FadeShare);
        _opacities.get(entity).value = std::clamp(left, 0.0f, 1.0f);
    }
}

} // namespace rtype::client
