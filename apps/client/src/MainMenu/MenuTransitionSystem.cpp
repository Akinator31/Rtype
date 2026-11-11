#include "MainMenu/MenuTransitionSystem.hpp"

#include "MainMenu/CameraFlight.hpp"
#include "MainMenu/IntroSystem.hpp"
#include "rtype/engine/Component/Animation.hpp"

#include <algorithm>

namespace rtype::client
{

MenuTransitionSystem::MenuTransitionSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs,
                                           const engine::Engine &engine, const engine::Scenes &scenes)
    : _engine(engine), _scenes(scenes), _ecs(ecs), _flights(use<MenuCameraFlight>(caches)),
      _intros(use<Intro>(caches)), _cameras(use<engine::Camera>(caches)), _fades(use<MenuFade>(caches)),
      _opacities(use<engine::Opacity>(caches)), _members(use<engine::SceneMember>(caches)),
      _drawables(use<engine::Drawable>(caches)), _flares(use<LogoFlare>(caches))
{
}

std::optional<std::size_t> MenuTransitionSystem::findDestination(const MenuCameraFlight &flight) const
{
    for (std::size_t index = 0; index < flight.views.size(); ++index) {
        if (_engine.isSceneOpen(flight.views[index].scene)) {
            return index;
        }
    }
    return std::nullopt;
}

bool MenuTransitionSystem::hasIntroStarted() const
{
    for (ecs::entity_t entity = 0; entity < _intros.size(); ++entity) {
        if (_intros.has(entity) && _intros.get(entity).started) {
            return true;
        }
    }
    return false;
}

void MenuTransitionSystem::fly(ecs::entity_t entity, MenuCameraFlight &flight, float elapsed)
{
    // The view of the menu is where its intro puts the camera: still on its way out of the asteroids if
    // the player was quick
    const render::Camera menu = computeIntroView(_intros.get(entity));
    const std::optional<std::size_t> destination = findDestination(flight);
    if (destination != flight.destination) {
        flight.from = flight.shown.value_or(menu);
        flight.destination = destination;
        flight.time = 0.0f;
    }
    flight.time = std::min(flight.time + elapsed, MenuFlightDuration);
    const render::Camera &to = destination ? flight.views[*destination].view : menu;
    const render::Camera view = mixViews(flight.from, to, easeInOut(flight.time / MenuFlightDuration));
    _cameras.get(entity).view = view;
    flight.shown = view;
}

/// The trail of the logo flare keeps its own opacities (LogoFlareSystem).
void MenuTransitionSystem::fadeIn(engine::SceneId scene)
{
    for (ecs::entity_t entity = 0; entity < _members.size(); ++entity) {
        const bool inInterface = _members.has(entity) && _members.get(entity).scene == scene &&
                                 _drawables.has(entity) &&
                                 _drawables.get(entity).layer == engine::Layer::Interface;
        if (!inInterface || _flares.has(entity) || _fades.has(entity)) {
            continue;
        }
        const float target = _opacities.has(entity) ? _opacities.get(entity).value : 1.0f;
        _ecs.addComponent<MenuFade>(entity, target);
        _ecs.addComponent<engine::Opacity>(entity, 0.0f);
    }
}

void MenuTransitionSystem::updateFades(float elapsed)
{
    for (ecs::entity_t entity = 0; entity < _fades.size(); ++entity) {
        if (!_fades.has(entity)) {
            continue;
        }
        MenuFade &fade = _fades.get(entity);
        fade.time += elapsed;
        const float progress =
            engine::ease(engine::Easing::EaseOut, std::min(fade.time / MenuFadeDuration, 1.0f));
        _ecs.addComponent<engine::Opacity>(entity, fade.target * progress);
        if (fade.time >= MenuFadeDuration) {
            _ecs.removeComponent<MenuFade>(entity);
        }
    }
}

void MenuTransitionSystem::update(float elapsed)
{
    for (ecs::entity_t entity = 0; entity < _flights.size(); ++entity) {
        if (_flights.has(entity) && _intros.has(entity) && _cameras.has(entity)) {
            fly(entity, _flights.get(entity), elapsed);
        }
    }
    const std::optional<engine::SceneId> top = _scenes.top();
    // Before its intro, the menu brings its elements in by itself
    if (top && top != _top && hasIntroStarted()) {
        fadeIn(*top);
    }
    _top = top;
    updateFades(elapsed);
}

} // namespace rtype::client
