#include "Loading/LoadingSystem.hpp"

#include <algorithm>

namespace rtype::client
{

namespace
{

/// How long the logo and the bar take to fade in, in seconds. The bar takes at least fillDuration to fill,
/// even when the assets load faster, stays full for fullDuration, then fades out in fadeOutDuration.
constexpr float fadeInDuration = 2.0f;
constexpr float fillDuration = 3.0f;
constexpr float fullDuration = 0.5f;
constexpr float fadeOutDuration = 0.8f;

/// The first frame lasts as long as everything done before it: the fades move by at most this much per
/// frame, so that they are seen from their start.
constexpr float longestFadeStep = 0.05f;

} // namespace

LoadingSystem::LoadingSystem(ecs::ComponentCaches &caches, AssetPreload &preload)
    : _preload(preload), _screens(use<LoadingScreen>(caches)), _opacities(use<engine::Opacity>(caches))
{
}

void LoadingSystem::fill(LoadingScreen &screen, float step)
{
    _preload.update();
    const float target = _preload.isDone() ? 1.0f : _preload.getProgress();
    screen.progress = std::min(target, screen.progress + (step / fillDuration));
}

void LoadingSystem::play(ecs::entity_t bar, LoadingScreen &screen, float elapsed)
{
    const float step = std::min(elapsed, longestFadeStep);
    screen.time += step;
    const float shown = std::min(screen.time / fadeInDuration, 1.0f);
    _opacities.emplace(screen.logo, shown);
    // Finishing a load takes the main thread for a while: done during the fade in, it would make it stutter
    if (shown >= 1.0f) {
        fill(screen, elapsed);
    }
    if (screen.leaving) {
        *screen.leaving += step;
    } else if (screen.progress >= 1.0f && shown >= 1.0f) {
        screen.leaving = 0.0f;
    }
    if (!screen.leaving) {
        _opacities.emplace(bar, shown);
        return;
    }
    const float hidden = std::clamp((*screen.leaving - fullDuration) / fadeOutDuration, 0.0f, 1.0f);
    _opacities.emplace(bar, 1.0f - hidden);
    _finished = _finished || hidden >= 1.0f;
}

void LoadingSystem::update(float elapsed)
{
    for (ecs::entity_t entity = 0; entity < _screens.size(); ++entity) {
        if (_screens.has(entity)) {
            play(entity, _screens.get(entity), elapsed);
        }
    }
}

bool LoadingSystem::isFinished() const
{
    return _finished;
}

} // namespace rtype::client
