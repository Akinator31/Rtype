#include "Game/ImpactSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rtype::client
{

ImpactSystem::ImpactSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs)
    : _ecs(ecs), _impacts(use<Impact>(caches)), _models(use<engine::Model>(caches)),
      _animations(use<engine::ModelAnimation>(caches))
{
}

void ImpactSystem::update(float elapsed)
{
    for (ecs::entity_t entity = 0; entity < _impacts.size(); ++entity) {
        if (!_impacts.has(entity)) {
            continue;
        }
        Impact &impact = _impacts.get(entity);
        impact.time += elapsed;
        if (impact.time >= impact.duration) {
            _ecs.destroyEntity(entity);
            continue;
        }
        playImpact(entity, impact);
    }
}

void ImpactSystem::playImpact(ecs::entity_t entity, Impact &impact)
{
    if (_animations.has(entity)) {
        _animations.get(entity).time = impact.time;
    }
    if (_models.has(entity)) {
        const float left = (impact.duration - impact.time) / (impact.duration * FadeShare);
        const float opacity = std::clamp(left, 0.0f, 1.0f);
        const long alpha = std::lround(static_cast<float>(impact.alpha) * opacity);
        _models.get(entity).tint.a = static_cast<std::uint8_t>(alpha);
    }
}

} // namespace rtype::client
