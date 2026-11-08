#include "rtype/voice/Talk.hpp"

#include "rtype/voice/Format.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace rtype::voice
{

namespace
{

/// The power of a sample at full scale.
constexpr double FullScale = static_cast<double>(std::numeric_limits<std::int16_t>::max()) + 1.0;

/// The level of a mean power, in dBFS.
float levelOfPower(double meanPower)
{
    if (meanPower <= 0.0) {
        return SilenceFloor;
    }
    const auto level = static_cast<float>(10.0 * std::log10(meanPower / (FullScale * FullScale)));
    return std::max(level, SilenceFloor);
}

double powerOf(std::span<const std::int16_t> samples)
{
    double power = 0.0;
    for (const std::int16_t sample : samples) {
        power += static_cast<double>(sample) * static_cast<double>(sample);
    }
    return power;
}

} // namespace

std::string_view name(TalkMode mode)
{
    switch (mode) {
    case TalkMode::PushToTalk:
        return "push_to_talk";
    case TalkMode::VoiceActivity:
        return "voice_activity";
    case TalkMode::Off:
        return "off";
    }
    return "off";
}

std::optional<TalkMode> talkModeFromName(std::string_view text)
{
    for (const TalkMode mode : TalkModes) {
        if (name(mode) == text) {
            return mode;
        }
    }
    return std::nullopt;
}

float levelOf(std::span<const std::int16_t> samples)
{
    if (samples.empty()) {
        return SilenceFloor;
    }
    return levelOfPower(powerOf(samples) / static_cast<double>(samples.size()));
}

void PushToTalk::update(bool held, TalkClock::time_point now)
{
    if (_held && !held) {
        _released = now;
    }
    _held = held;
}

bool PushToTalk::talking(TalkClock::time_point now) const
{
    return _held || (_released && now - *_released < ReleaseDelay);
}

bool VoiceActivity::update(float level, float threshold)
{
    if (level >= threshold) {
        _hold = Hold;
        return true;
    }
    if (_hold == 0) {
        return false;
    }
    --_hold;
    return true;
}

void LevelMeter::push(std::span<const std::int16_t> samples)
{
    while (!samples.empty()) {
        const std::size_t taken = std::min(samples.size(), FrameSamples - _count);
        _power += powerOf(samples.first(taken));
        _count += taken;
        samples = samples.subspan(taken);
        if (_count == FrameSamples) {
            const float frame = levelOfPower(_power / static_cast<double>(FrameSamples));
            _level = std::max(frame, std::max(_level - FallPerFrame, SilenceFloor));
            _power = 0.0;
            _count = 0;
        }
    }
}

float levelFraction(float level, float low, float high)
{
    return std::clamp((level - low) / (high - low), 0.0f, 1.0f);
}

float levelAt(float fraction, float low, float high)
{
    return low + (std::clamp(fraction, 0.0f, 1.0f) * (high - low));
}

} // namespace rtype::voice
