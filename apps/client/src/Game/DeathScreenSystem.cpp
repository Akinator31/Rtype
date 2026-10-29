#include "Game/DeathScreenSystem.hpp"

#include "rtype/engine/Component/Animation.hpp"
#include "rtype/game/Constants.hpp"

#include <algorithm>
#include <string>

namespace rtype::client
{

namespace
{

/// 0 before `delay`, then up to 1 in `duration` seconds.
float appear(float time, float delay, float duration)
{
    return std::clamp((time - delay) / duration, 0.0f, 1.0f);
}

} // namespace

DeathScreenSystem::DeathScreenSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs,
                                     const GameSessionSystem &sessions)
    : _ecs(ecs), _sessions(sessions), _screens(use<DeathScreen>(caches)),
      _opacities(use<engine::Opacity>(caches)), _texts(use<engine::Text>(caches)),
      _rings(use<engine::Ring>(caches))
{
}

void DeathScreenSystem::update(float elapsed)
{
    for (ecs::entity_t entity = 0; entity < _screens.size(); ++entity) {
        if (!_screens.has(entity)) {
            continue;
        }
        DeathScreen &screen = _screens.get(entity);
        screen.time += elapsed;
        const std::int64_t ticksLeft =
            static_cast<std::int64_t>(screen.death.respawnTick) - static_cast<std::int64_t>(_sessions.tick());
        if (screen.closing) {
            *screen.closing += elapsed;
        } else if (ticksLeft <= 0) {
            screen.closing = 0.0f;
        }
        if (screen.closing && *screen.closing >= FadeOut) {
            close(entity, screen);
            continue;
        }
        showCountdown(screen, ticksLeft);
        fade(screen);
    }
}

void DeathScreenSystem::showCountdown(const DeathScreen &screen, std::int64_t ticksLeft)
{
    if (ticksLeft <= 0) {
        // The ship is back: the last second stays while the screen fades out
        _rings.get(screen.ring).value = 0.0f;
        return;
    }
    const std::uint32_t delay = std::max<std::uint32_t>(screen.death.respawnDelay, 1);
    _rings.get(screen.ring).value = static_cast<float>(ticksLeft) / static_cast<float>(delay);
    const std::string seconds = std::to_string((ticksLeft + game::TickRate - 1) / game::TickRate);
    engine::Text &countdown = _texts.get(screen.countdown);
    if (countdown.text == seconds) {
        return;
    }
    countdown.text = seconds;
    _ecs.addComponent<engine::Tween>(screen.countdown, engine::Tween{.target = engine::TweenTarget::Scale,
                                                                     .from = PopScale,
                                                                     .to = 1.0f,
                                                                     .duration = PopDuration,
                                                                     .easing = engine::Easing::EaseOut});
}

void DeathScreenSystem::fade(const DeathScreen &screen)
{
    const float out = screen.closing ? 1.0f - (*screen.closing / FadeOut) : 1.0f;
    const float content = appear(screen.time, ContentDelay, ContentIn) * out;
    setOpacity(screen.veil, appear(screen.time, 0.0f, VeilIn) * out);
    for (const ecs::entity_t part : screen.content) {
        setOpacity(part, content);
    }
    setOpacity(screen.lostLife, content * (1.0f - appear(screen.time, LifeOutDelay, LifeOut)));
}

void DeathScreenSystem::setOpacity(ecs::entity_t entity, float opacity)
{
    if (_opacities.has(entity)) {
        _opacities.get(entity).value = opacity;
    }
}

void DeathScreenSystem::close(ecs::entity_t entity, const DeathScreen &screen)
{
    _ecs.destroyEntity(screen.veil);
    for (const ecs::entity_t part : screen.content) {
        _ecs.destroyEntity(part);
    }
    _ecs.destroyEntity(screen.lostLife);
    _ecs.destroyEntity(entity);
}

} // namespace rtype::client
