#include "rtype/engine/System/TweenSystem.hpp"

#include <algorithm>
#include <cmath>

namespace rtype::engine
{

TweenSystem::TweenSystem(ecs::ComponentCaches &caches)
    : _tweens(use<Tween>(caches)), _scales(use<Scale>(caches)), _opacities(use<Opacity>(caches))
{
}

void TweenSystem::update(float elapsed)
{
    for (ecs::entity_t entity = 0; entity < _tweens.size(); ++entity) {
        if (!_tweens.has(entity)) {
            continue;
        }
        Tween &tween = _tweens.get(entity);
        tween.time += elapsed;
        const float duration = std::max(tween.duration, 0.0001f);
        float progress = tween.time / duration;
        bool finished = false;
        if (tween.pingPong) {
            // 0 -> 1 -> 0 -> 1...: the fractional part of progress / 2, folded
            const float cycle = std::fmod(progress, 2.0f);
            progress = cycle <= 1.0f ? cycle : 2.0f - cycle;
        } else if (progress >= 1.0f) {
            progress = 1.0f;
            finished = true;
        }
        const float value = tween.from + (tween.to - tween.from) * ease(tween.easing, progress);

        if (tween.target == TweenTarget::Scale) {
            _scales.emplace(entity, value);
        } else {
            _opacities.emplace(entity, std::clamp(value, 0.0f, 1.0f));
        }
        if (finished) {
            _tweens.erase(entity);
        }
    }
}

} // namespace rtype::engine
