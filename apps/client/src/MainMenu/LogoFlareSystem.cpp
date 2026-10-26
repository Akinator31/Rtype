#include "MainMenu/LogoFlareSystem.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace rtype::client
{

namespace
{

constexpr float fullTurn = 2.0f * std::numbers::pi_v<float>;

/// The flare comes from out of the screen, spiralStart seconds after the press: spiralStartRadius times the
/// radius of the ring away from its centre, spiralStartAngle radians clockwise from its top (on the bottom
/// left). It spirals clockwise into the ring, faster and faster, rolling on it, lands on it after
/// spiralLanding of the spiral, and goes on around it to its top. It shrinks from spiralStartScale to
/// orbitScale on the way.
constexpr float spiralStart = 0.2f;
constexpr float spiralDuration = 2.4f;
constexpr float spiralStartRadius = 7.0f;
constexpr float spiralStartAngle = -0.75f * std::numbers::pi_v<float>;
constexpr float spiralLanding = 0.55f;
constexpr float spiralStartScale = 1.6f;
constexpr float orbitScale = 0.9f;

/// On the top of the ring, it leaves it towards the right and swoops down to its place, slowing down. The
/// curve passes near a point at swoopBend of the way from the top of the ring to the place, at the height of
/// the top: it leaves the ring along it.
constexpr float swoopDuration = 0.8f;
constexpr float swoopBend = 0.7f;

/// When it lands, it flashes: it grows to flashScale and twists by flashTwist degrees in the first
/// flashPeak of the flash, then comes back to its size and to straight.
constexpr float flashDuration = 0.8f;
constexpr float flashScale = 3.0f;
constexpr float flashTwist = 45.0f;
constexpr float flashPeak = 0.15f;

/// Once landed, its size swings by twinkleAmount every twinklePeriod seconds.
constexpr float twinkleAmount = 0.06f;
constexpr float twinklePeriod = 2.4f;

/// A trail fades out over the end of the swoop, from this part of it.
constexpr float trailFadeFrom = 0.4f;

constexpr float swoopStart = spiralStart + spiralDuration;
constexpr float flashStart = swoopStart + swoopDuration;
constexpr float twinkleStart = flashStart + flashDuration;

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

/// The point `distance` away from the centre of the ring, at `angle` radians clockwise from its top.
[[nodiscard]] render::Vector2 getPointAroundRing(const LogoFlare &flare, float angle, float distance)
{
    return {flare.ringCentre.x + (distance * std::sin(angle)),
            flare.ringCentre.y - (distance * std::cos(angle))};
}

[[nodiscard]] render::Vector2 getRingPoint(const LogoFlare &flare, float angle)
{
    return getPointAroundRing(flare, angle, flare.ringRadius);
}

/// The point at `progress` of the curve from the top of the ring to the place.
[[nodiscard]] render::Vector2 getSwoopPoint(const LogoFlare &flare, float progress)
{
    const render::Vector2 top = getRingPoint(flare, 0.0f);
    const render::Vector2 bend{mix(top.x, flare.place.x, swoopBend), top.y};
    return mix(mix(top, bend, progress), mix(bend, flare.place, progress), progress);
}

/// The distance from the centre of the ring shrinks more and more slowly, so that the flare lands on the ring
/// along it.
[[nodiscard]] float computeSpiralDistance(const LogoFlare &flare, float progress)
{
    const float remaining = 1.0f - std::min(progress / spiralLanding, 1.0f);
    return flare.ringRadius * mix(1.0f, spiralStartRadius, remaining * remaining);
}

[[nodiscard]] FlarePose computeSpiral(const LogoFlare &flare, float time)
{
    const float progress = getProgress(time, spiralStart, spiralDuration);
    const float turning = (progress + (progress * progress)) / 2.0f;
    const float angle = mix(spiralStartAngle, fullTurn, turning);
    return {.offset = getPointAroundRing(flare, angle, computeSpiralDistance(flare, progress)),
            .scale = mix(spiralStartScale, orbitScale, progress),
            .opacity = 1.0f,
            .rotation = angle * 180.0f / std::numbers::pi_v<float>};
}

[[nodiscard]] FlarePose computeSwoop(const LogoFlare &flare, float time)
{
    const float progress = getProgress(time, swoopStart, swoopDuration);
    const float slowing = 1.0f - ((1.0f - progress) * (1.0f - progress));
    return {.offset = getSwoopPoint(flare, slowing),
            .scale = mix(orbitScale, 1.0f, progress),
            .opacity = 1.0f,
            .rotation = 0.0f};
}

[[nodiscard]] FlarePose computeFlash(const LogoFlare &flare, float time)
{
    const float progress = getProgress(time, flashStart, flashDuration);
    const float fall = 1.0f - ((progress - flashPeak) / (1.0f - flashPeak));
    const float burst = progress < flashPeak ? progress / flashPeak : fall * fall;
    return {.offset = flare.place,
            .scale = mix(1.0f, flashScale, burst),
            .opacity = 1.0f,
            .rotation = flashTwist * burst};
}

[[nodiscard]] FlarePose computeTwinkle(const LogoFlare &flare, float time)
{
    const float wave = std::sin(fullTurn * (time - twinkleStart) / twinklePeriod);
    return {.offset = flare.place, .scale = 1.0f + (twinkleAmount * wave), .opacity = 1.0f, .rotation = 0.0f};
}

[[nodiscard]] FlarePose computeFlarePose(const LogoFlare &flare, float time)
{
    if (time < swoopStart) {
        return computeSpiral(flare, time);
    }
    if (time < flashStart) {
        return computeSwoop(flare, time);
    }
    if (time < twinkleStart) {
        return computeFlash(flare, time);
    }
    return computeTwinkle(flare, time);
}

/// A trail is the flare `lag` seconds earlier, smaller and fainter, gone once the flare has landed.
[[nodiscard]] FlarePose computeTrailPose(const LogoFlare &flare, float time)
{
    const float trailTime = time - flare.lag;
    FlarePose pose = computeFlarePose(flare, trailTime);
    const float fade = 1.0f - getProgress(trailTime, swoopStart + (trailFadeFrom * swoopDuration),
                                          (1.0f - trailFadeFrom) * swoopDuration);
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
    const FlarePose pose = flare.lag > 0.0f ? computeTrailPose(flare, time) : computeFlarePose(flare, time);
    const engine::Anchor &logo = _anchors.get(flare.logo);
    const float logoScale = _scales.has(flare.logo) ? _scales.get(flare.logo).value : 1.0f;
    engine::Anchor &anchor = _anchors.get(entity);
    anchor.x = logo.x + (pose.offset.x * logoScale);
    anchor.y = logo.y + (pose.offset.y * logoScale);
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
