#include "Game/HealthBarSystem.hpp"

#include <algorithm>

namespace rtype::client
{

namespace
{

/// The share the bar shows: it goes down to the hit points left; healed, it goes up with them at once.
float drain(const HealthBar &health, float elapsed)
{
    return std::max(health.health, health.shown - (HealthBarSystem::DrainSpeed * elapsed));
}

/// Opaque for a while after the last hit, then fading out.
float opacityOf(const HealthBar &health)
{
    const float faded = (health.time - HealthBarSystem::ShowDuration) / HealthBarSystem::FadeDuration;
    return std::clamp(1.0f - faded, 0.0f, 1.0f);
}

} // namespace

HealthBarSystem::HealthBarSystem(ecs::ComponentCaches &caches)
    : _bars(use<HealthBar>(caches)), _opacities(use<engine::Opacity>(caches))
{
}

void HealthBarSystem::update(float elapsed)
{
    for (ecs::entity_t entity = 0; entity < _bars.size(); ++entity) {
        if (!_bars.has(entity)) {
            continue;
        }
        HealthBar &health = _bars.get(entity);
        health.time += elapsed;
        health.shown = drain(health, elapsed);
        if (_opacities.has(entity)) {
            _opacities.get(entity).value = opacityOf(health);
        }
    }
}

} // namespace rtype::client
