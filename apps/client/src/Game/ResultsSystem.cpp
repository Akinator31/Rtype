#include "Game/ResultsSystem.hpp"

#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Sound.hpp"
#include "rtype/game/Progression.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

namespace rtype::client
{

namespace
{

/// 0 before `delay`, then up to 1 in `duration` seconds.
float appear(float time, float delay, float duration)
{
    return std::clamp((time - delay) / duration, 0.0f, 1.0f);
}

/// Fast first, then slower and slower as it nears the end: the last figures can be read.
float easeOut(float progress)
{
    const float left = 1.0f - progress;
    return 1.0f - (left * left * left);
}

} // namespace

ResultsSystem::ResultsSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs)
    : _ecs(ecs), _screens(use<ResultsScreen>(caches)), _opacities(use<engine::Opacity>(caches)),
      _texts(use<engine::Text>(caches)), _boxes(use<engine::Box>(caches)),
      _anchors(use<engine::Anchor>(caches))
{
}

void ResultsSystem::update(float elapsed)
{
    for (ecs::entity_t entity = 0; entity < _screens.size(); ++entity) {
        if (!_screens.has(entity)) {
            continue;
        }
        ResultsScreen &screen = _screens.get(entity);
        if (!std::exchange(screen.opened, true)) {
            play(screen.title, screen.sounds.gameOver, GameOverVolume);
        }
        screen.time += elapsed;
        fade(screen);
        land(screen);
        count(screen, elapsed);
    }
}

void ResultsSystem::fade(const ResultsScreen &screen)
{
    setOpacity(screen.veil, appear(screen.time, 0.0f, TitleIn));
    setOpacity(screen.title, appear(screen.time, 0.0f, TitleIn));
    for (std::size_t index = 0; index < screen.figures.size(); ++index) {
        const float opacity =
            appear(screen.time, FiguresDelay + (FiguresGap * static_cast<float>(index)), FigureIn);
        setOpacity(screen.figures[index], opacity);
        setOpacity(screen.captions[index], opacity);
    }
    const float experience = appear(screen.time, CountDelay - FigureIn, FigureIn);
    for (const ecs::entity_t part : screen.experience) {
        setOpacity(part, experience);
    }
    if (screen.guest) {
        setOpacity(*screen.guest, appear(screen.time, CountDelay + CountDuration, TitleIn));
    }
}

void ResultsSystem::land(ResultsScreen &screen)
{
    while (screen.landed < screen.figures.size() &&
           screen.time >= FiguresDelay + (FiguresGap * static_cast<float>(screen.landed))) {
        pop(screen.figures[screen.landed], PopScale);
        play(screen.figures[screen.landed], screen.sounds.stat, StatVolume);
        ++screen.landed;
    }
}

void ResultsSystem::count(ResultsScreen &screen, float elapsed)
{
    if (screen.time < CountDelay - FigureIn) {
        return;
    }
    const float progress = easeOut(appear(screen.time, CountDelay, CountDuration));
    const auto xp = static_cast<std::uint32_t>(std::lround(static_cast<double>(screen.game.xp) * progress));
    screen.sinceTick += elapsed;
    if (xp != screen.xpShown && screen.sinceTick >= TickInterval) {
        screen.sinceTick = 0.0f;
        play(screen.xp, screen.sounds.tick, TickVolume, 1.0f + (TickRise * progress));
    }
    screen.xpShown = xp;
    _texts.get(screen.xp).text = std::format("+{} XP", xp);

    if (screen.before) {
        // The master may tell the experience after the game while it counts: the bar follows it
        const std::uint64_t gained = screen.after - *screen.before;
        const std::uint64_t account =
            *screen.before + static_cast<std::uint64_t>(std::llround(static_cast<double>(gained) * progress));
        showLevel(screen, account);
        const std::uint32_t level = game::levelOf(account).level;
        if (level > screen.levelShown) {
            screen.levelShown = level;
            setOpacity(screen.levelUp, 1.0f);
            pop(screen.levelUp, PopScale);
            play(screen.levelUp, screen.sounds.levelUp, LevelUpVolume);
        }
    }
    if (progress >= 1.0f && !screen.counted) {
        screen.counted = true;
        pop(screen.xp, PopScale);
        play(screen.xpCaption, screen.sounds.counted, CountedVolume);
    }
}

void ResultsSystem::showLevel(const ResultsScreen &screen, std::uint64_t xp)
{
    const game::Level level = game::levelOf(xp);
    _texts.get(screen.level).text = std::format("{} {}", screen.levelCaption, level.level);
    _texts.get(screen.levelXp).text = std::format("{} / {} XP", level.xpIntoLevel, level.xpForNext);
    const float lit = level.xpForNext == 0 ? 0.0f
                                           : static_cast<float>(static_cast<double>(level.xpIntoLevel) /
                                                                static_cast<double>(level.xpForNext));
    const float width = ResultsBarWidth * std::clamp(lit, 0.0f, 1.0f);
    _boxes.get(screen.barFill).width = width;
    // Centred on its anchor: lit from the left end of the bar
    _anchors.get(screen.barFill).x = (-ResultsBarWidth + width) / 2.0f;
}

void ResultsSystem::setOpacity(ecs::entity_t entity, float opacity)
{
    if (_opacities.has(entity)) {
        _opacities.get(entity).value = opacity;
    }
}

void ResultsSystem::pop(ecs::entity_t entity, float scale)
{
    _ecs.addComponent<engine::Tween>(entity, engine::Tween{.target = engine::TweenTarget::Scale,
                                                           .from = scale,
                                                           .to = 1.0f,
                                                           .duration = PopDuration,
                                                           .easing = engine::Easing::EaseOut});
}

void ResultsSystem::play(ecs::entity_t entity, audio::SoundId sound, float volume, float pitch)
{
    _ecs.addComponent<engine::SoundEffect>(entity, sound,
                                           audio::PlaySettings{.volume = volume, .pitch = pitch});
}

} // namespace rtype::client
