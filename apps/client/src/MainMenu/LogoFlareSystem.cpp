#include "MainMenu/LogoFlareSystem.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace rtype::client
{

namespace
{

constexpr float fullTurn = 2.0f * std::numbers::pi_v<float>;

/// The flare is thrown throwStart seconds after the press from throwFrom, in game units on the screen from
/// the centre of the logo: out of the left of a 16:9 screen, under the height of its place. Like a thrown
/// stone, it moves to the right at constant speed while it rises then falls, along a parabola whose top is
/// throwHeight game units above the middle of the line from the start to the place, and lands after
/// throwDuration seconds. On the way, it shrinks from throwScale to its size and spins throwTurns times
/// clockwise at constant speed, to land straight.
constexpr float throwStart = 0.2f;
constexpr float throwDuration = 1.6f;
constexpr render::Vector2 throwFrom{-1200.0f, 250.0f};
constexpr float throwHeight = 380.0f;
constexpr float throwScale = 1.4f;
constexpr float throwTurns = 2.0f;

/// When it lands, it flashes: it grows to flashScale and twists by flashTwist degrees in the first
/// flashPeak of the flash, then comes back to its size and to straight.
constexpr float flashDuration = 0.8f;
constexpr float flashScale = 3.0f;
constexpr float flashTwist = 45.0f;
constexpr float flashPeak = 0.15f;

/// Once landed, its size swings by twinkleAmount every twinklePeriod seconds.
constexpr float twinkleAmount = 0.06f;
constexpr float twinklePeriod = 2.4f;

/// A trail fades out over the end of the throw, from this part of it.
constexpr float trailFadeFrom = 0.6f;

constexpr float flashStart = throwStart + throwDuration;
constexpr float twinkleStart = flashStart + flashDuration;

/// Where the flare is and how it looks: `offset` from the centre of the logo in game units, on the screen
/// (the points that belong to the logo are already shrunk with it), and `scale` compared with the logo.
struct FlarePose {
    render::Vector2 offset;
    float scale = 1.0f;
    float opacity = 1.0f;
    /// Clockwise, in degrees.
    float rotation = 0.0f;
};

[[nodiscard]] float mix(float from, float to, float amount)
{
    return from + ((to - from) * amount);
}

[[nodiscard]] render::Vector2 mix(render::Vector2 from, render::Vector2 to, float amount)
{
    return {mix(from.x, to.x, amount), mix(from.y, to.y, amount)};
}

[[nodiscard]] float getProgress(float time, float start, float duration)
{
    return std::clamp((time - start) / duration, 0.0f, 1.0f);
}

/// The point at `progress` of the parabola from the left of the screen to the place.
[[nodiscard]] render::Vector2 getThrowPoint(const LogoFlare &flare, float progress, float logoScale)
{
    const render::Vector2 place{flare.place.x * logoScale, flare.place.y * logoScale};
    const render::Vector2 line = mix(throwFrom, place, progress);
    const float rise = 4.0f * throwHeight * progress * (1.0f - progress);
    return {line.x, line.y - rise};
}

[[nodiscard]] FlarePose computeThrow(const LogoFlare &flare, float time, float logoScale)
{
    const float progress = getProgress(time, throwStart, throwDuration);
    return {.offset = getThrowPoint(flare, progress, logoScale),
            .scale = mix(throwScale, 1.0f, progress),
            .opacity = 1.0f,
            .rotation = throwTurns * 360.0f * (1.0f - progress)};
}

[[nodiscard]] FlarePose computeFlash(const LogoFlare &flare, float time, float logoScale)
{
    const float progress = getProgress(time, flashStart, flashDuration);
    const float fall = 1.0f - ((progress - flashPeak) / (1.0f - flashPeak));
    const float burst = progress < flashPeak ? progress / flashPeak : fall * fall;
    return {.offset = {flare.place.x * logoScale, flare.place.y * logoScale},
            .scale = mix(1.0f, flashScale, burst),
            .opacity = 1.0f,
            .rotation = flashTwist * burst};
}

[[nodiscard]] FlarePose computeTwinkle(const LogoFlare &flare, float time, float logoScale)
{
    const float wave = std::sin(fullTurn * (time - twinkleStart) / twinklePeriod);
    return {.offset = {flare.place.x * logoScale, flare.place.y * logoScale},
            .scale = 1.0f + (twinkleAmount * wave),
            .opacity = 1.0f,
            .rotation = 0.0f};
}

[[nodiscard]] FlarePose computeFlarePose(const LogoFlare &flare, float time, float logoScale)
{
    if (time < flashStart) {
        return computeThrow(flare, time, logoScale);
    }
    if (time < twinkleStart) {
        return computeFlash(flare, time, logoScale);
    }
    return computeTwinkle(flare, time, logoScale);
}

/// A trail is the flare `lag` seconds earlier, smaller and fainter, gone once the flare has landed.
[[nodiscard]] FlarePose computeTrailPose(const LogoFlare &flare, float time, float logoScale)
{
    const float trailTime = time - flare.lag;
    FlarePose pose = computeFlarePose(flare, trailTime, logoScale);
    const float fade = 1.0f - getProgress(trailTime, throwStart + (trailFadeFrom * throwDuration),
                                          (1.0f - trailFadeFrom) * throwDuration);
    pose.scale *= flare.strength;
    pose.opacity *= flare.strength * fade;
    return pose;
}

} // namespace

LogoFlareSystem::LogoFlareSystem(ecs::ComponentCaches &caches)
    : _flares(use<LogoFlare>(caches)), _intros(use<Intro>(caches)), _anchors(use<engine::Anchor>(caches)),
      _scales(use<engine::Scale>(caches)), _opacities(use<engine::Opacity>(caches)),
      _sprites(use<engine::Sprite>(caches))
{
}

float LogoFlareSystem::getIntroTime(const LogoFlare &flare) const
{
    if (!_intros.has(flare.intro) || !_intros.get(flare.intro).started) {
        return 0.0f;
    }
    return _intros.get(flare.intro).time;
}

void LogoFlareSystem::placeFlare(ecs::entity_t entity, const LogoFlare &flare)
{
    const float time = getIntroTime(flare);
    const float logoScale = _scales.has(flare.logo) ? _scales.get(flare.logo).value : 1.0f;
    const FlarePose pose = flare.lag > 0.0f ? computeTrailPose(flare, time, logoScale)
                                            : computeFlarePose(flare, time, logoScale);
    const engine::Anchor &logo = _anchors.get(flare.logo);
    engine::Anchor &anchor = _anchors.get(entity);
    anchor.x = logo.x + pose.offset.x;
    anchor.y = logo.y + pose.offset.y;
    _scales.emplace(entity, logoScale * pose.scale);
    _opacities.emplace(entity, pose.opacity);
    _sprites.get(entity).rotation = pose.rotation;
}

void LogoFlareSystem::update(float /*elapsed*/)
{
    for (ecs::entity_t entity = 0; entity < _flares.size(); ++entity) {
        if (!_flares.has(entity) || !_anchors.has(entity) || !_sprites.has(entity)) {
            continue;
        }
        const LogoFlare &flare = _flares.get(entity);
        if (_anchors.has(flare.logo)) {
            placeFlare(entity, flare);
        }
    }
}

} // namespace rtype::client
