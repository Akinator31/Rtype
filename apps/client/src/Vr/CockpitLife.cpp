#include "Vr/CockpitLife.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>

namespace rtype::client
{

namespace
{

/// A colour multiplied by a brightness, from 0 to 1.
render::Color scaled(render::Color color, float brightness)
{
    const float kept = std::clamp(brightness, 0.0f, 1.0f);
    const auto channel = [kept](std::uint8_t value) {
        return static_cast<std::uint8_t>(std::lround(static_cast<float>(value) * kept));
    };
    return {channel(color.r), channel(color.g), channel(color.b), 255};
}

/// From one colour (0) to another (1).
render::Color mixed(render::Color from, render::Color to, float amount)
{
    const float kept = std::clamp(amount, 0.0f, 1.0f);
    const auto channel = [kept](std::uint8_t first, std::uint8_t second) {
        return static_cast<std::uint8_t>(
            std::lround(static_cast<float>(first) + ((static_cast<float>(second) - first) * kept)));
    };
    return {channel(from.r, to.r), channel(from.g, to.g), channel(from.b, to.b), 255};
}

/// From 0 to 1 and back, once every period, 1 at the start.
float pulse(float time, float period)
{
    return 0.5f + (0.5f * std::cos(2.0f * std::numbers::pi_v<float> * time / period));
}

/// Whether a screen is lit at a moment of its flicker, a broken pattern of about 12 changes per second: the
/// screens do not flicker together.
bool flickerLit(float time, std::size_t screen)
{
    constexpr float Changes = 12.0f;
    const auto step = static_cast<std::size_t>(time * Changes);
    return ((step * 7) + (screen * 3)) % 5 > 1;
}

} // namespace

void CockpitLife::update(float elapsed, const CockpitFeel &feel)
{
    _alarm = false;
    if (!feel.playing) {
        _previous.reset();
        _lights = {};
        return;
    }
    const float step = std::max(elapsed, 0.0f);
    if (!_previous) {
        // A game that starts: the cockpit starts too, its ship whole
        _sinceStart = 0.0f;
        _sinceHit.reset();
        _sinceDeath.reset();
        _lowTime = 0.0f;
        _nextAlarm = 0.0f;
        _bossTime = 0.0f;
    } else {
        _sinceStart += step;
        for (std::optional<float> *since : {&_sinceHit, &_sinceDeath}) {
            if (*since) {
                **since += step;
            }
        }
        if (_previous->health > 0 && feel.health < _previous->health) {
            if (feel.health == 0) {
                _sinceDeath = 0.0f;
                _sinceHit.reset();
            } else {
                _sinceHit = 0.0f;
                _alarm = true;
                _nextAlarm = LowLifeAlarm;
            }
        }
        // The ship came back: its screens with it
        if (_previous->health == 0 && feel.health > 0) {
            _sinceDeath.reset();
        }
    }
    const bool low = feel.health > 0 && feel.maxHealth > 0 &&
                     static_cast<float>(feel.health) <= static_cast<float>(feel.maxHealth) * LowLife;
    if (low) {
        _lowTime += step;
        _nextAlarm -= step;
        if (_nextAlarm <= 0.0f) {
            _alarm = true;
            _nextAlarm += LowLifeAlarm;
        }
    } else {
        _lowTime = 0.0f;
        _nextAlarm = 0.0f;
    }
    _bossTime = feel.boss ? _bossTime + step : 0.0f;
    _lights.brightness = std::clamp(_sinceStart / CockpitBootFade, 0.0f, 1.0f);
    lightGlows(feel);
    lightScreens();
    lightCanopy(feel);
    _previous = feel;
}

void CockpitLife::lightGlows(const CockpitFeel &feel)
{
    float cyan = 1.0f;
    render::Color orange = render::colors::White;
    if (feel.boss) {
        orange = scaled(render::colors::White, 0.35f + (0.65f * pulse(_bossTime, BossPulse)));
        cyan = 0.7f;
    }
    const bool low = _lowTime > 0.0f;
    if (low) {
        orange = scaled(AlertRed, 0.35f + (0.65f * pulse(_lowTime, LowLifePulse)));
        cyan = 0.5f;
    }
    if (_sinceHit && *_sinceHit < HitFlash) {
        const float flash = 1.0f - (*_sinceHit / HitFlash);
        orange = mixed(orange, AlertRed, flash);
        cyan += (0.25f - cyan) * flash;
    }
    if (_sinceDeath) {
        orange = scaled(render::colors::White, 0.3f);
        cyan = 0.3f;
    }
    // The glows light up with the cockpit
    _lights.cyan = scaled(render::colors::White, cyan * _lights.brightness);
    _lights.orange = scaled(orange, _lights.brightness);
}

void CockpitLife::lightScreens()
{
    for (std::size_t index = 0; index < _lights.screens.size(); ++index) {
        const float start = CockpitFirstScreen + (static_cast<float>(index) * CockpitScreenGap);
        float level = std::clamp((_sinceStart - start) / CockpitScreenLighting, 0.0f, 1.0f);
        if (_sinceDeath) {
            level *=
                *_sinceDeath < DeathFlicker ? (flickerLit(*_sinceDeath, index) ? 1.0f : 0.1f) : DeadScreens;
        }
        _lights.screens.at(index) = scaled(render::colors::White, level);
    }
}

void CockpitLife::lightCanopy(const CockpitFeel &feel)
{
    // It rises in 0.3 s, then fades until CanopyGlowEnd
    constexpr float Rise = 0.3f;
    const float glow = _sinceStart < Rise
                           ? _sinceStart / Rise
                           : std::max(0.0f, 1.0f - ((_sinceStart - Rise) / (CanopyGlowEnd - Rise)));
    _lights.canopy = scaled(feel.pilot, CanopyGlow * glow);
}

bool CockpitLife::takeAlarm()
{
    return std::exchange(_alarm, false);
}

} // namespace rtype::client
