#include "rtype/voice/Format.hpp"
#include "rtype/voice/Talk.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <numbers>

using namespace std::chrono_literals;
using Catch::Approx;
using rtype::voice::FrameSamples;
using rtype::voice::LevelMeter;
using rtype::voice::PushToTalk;
using rtype::voice::TalkClock;
using rtype::voice::TalkMode;
using rtype::voice::VoiceActivity;

namespace
{

using Frame = std::array<std::int16_t, FrameSamples>;

/// A sine of 440 Hz at this fraction of full scale.
Frame sine(double amplitude)
{
    Frame frame{};
    for (std::size_t index = 0; index < frame.size(); ++index) {
        const double phase =
            2.0 * std::numbers::pi * 440.0 * static_cast<double>(index) / rtype::voice::SampleRate;
        frame[index] = static_cast<std::int16_t>(std::sin(phase) * amplitude * 32767.0);
    }
    return frame;
}

const TalkClock::time_point Start = TalkClock::now();

} // namespace

TEST_CASE("The talk modes are written and read back by their names", "[voice]")
{
    for (const TalkMode mode : rtype::voice::TalkModes) {
        CHECK(rtype::voice::talkModeFromName(rtype::voice::name(mode)) == mode);
    }
    CHECK(rtype::voice::name(TalkMode::PushToTalk) == "push_to_talk");
    CHECK_FALSE(rtype::voice::talkModeFromName("shout").has_value());
}

TEST_CASE("The level of samples is in dBFS, silence at the floor", "[voice]")
{
    CHECK(rtype::voice::levelOf(Frame{}) == rtype::voice::SilenceFloor);
    CHECK(rtype::voice::levelOf({}) == rtype::voice::SilenceFloor);
    // A sine at full scale is about -3 dBFS, ten times quieter 20 dB lower
    CHECK(rtype::voice::levelOf(sine(1.0)) == Approx(-3.01f).margin(0.1f));
    CHECK(rtype::voice::levelOf(sine(0.1)) == Approx(-23.01f).margin(0.1f));
}

TEST_CASE("Push-to-talk talks while the key is held, and 200 ms after it is released", "[voice]")
{
    PushToTalk key;
    CHECK_FALSE(key.talking(Start));
    key.update(true, Start);
    CHECK(key.talking(Start + 5s));
    key.update(false, Start + 5s);
    CHECK(key.talking(Start + 5s + 199ms));
    CHECK_FALSE(key.talking(Start + 5s + PushToTalk::ReleaseDelay));
    // Held again within the delay: it goes on
    key.update(true, Start + 6s);
    key.update(false, Start + 7s);
    key.update(true, Start + 7s + 100ms);
    CHECK(key.talking(Start + 9s));
}

TEST_CASE("The voice activity hears the frames over the threshold, and 300 ms after", "[voice]")
{
    VoiceActivity activity;
    CHECK_FALSE(activity.update(-50.0f, -40.0f));
    CHECK(activity.update(-30.0f, -40.0f));
    // The quiet end of a word, and a short pause
    for (std::size_t frame = 0; frame < VoiceActivity::Hold; ++frame) {
        CHECK(activity.update(-60.0f, -40.0f));
    }
    CHECK_FALSE(activity.update(-60.0f, -40.0f));

    CHECK(activity.update(-40.0f, -40.0f));
    activity.reset();
    CHECK_FALSE(activity.update(-60.0f, -40.0f));
}

TEST_CASE("The level meter rises at once and falls at 60 dB per second", "[voice]")
{
    LevelMeter meter;
    CHECK(meter.level() == rtype::voice::SilenceFloor);
    const Frame loud = sine(1.0);
    meter.push(loud);
    CHECK(meter.level() == Approx(-3.01f).margin(0.1f));
    // Half a frame moves nothing, the other half does
    const Frame quiet{};
    meter.push(std::span<const std::int16_t>(quiet).first(FrameSamples / 2));
    CHECK(meter.level() == Approx(-3.01f).margin(0.1f));
    meter.push(std::span<const std::int16_t>(quiet).first(FrameSamples / 2));
    CHECK(meter.level() == Approx(-3.01f - LevelMeter::FallPerFrame).margin(0.1f));
    // A second of silence: 60 dB lower
    for (std::size_t frame = 1; frame < rtype::voice::FramesPerSecond; ++frame) {
        meter.push(quiet);
    }
    CHECK(meter.level() == Approx(-63.01f).margin(0.2f));
    meter.reset();
    CHECK(meter.level() == rtype::voice::SilenceFloor);
}

TEST_CASE("The meter and the threshold share a scale from -70 to -10 dBFS", "[voice]")
{
    CHECK(rtype::voice::levelFraction(-70.0f) == 0.0f);
    CHECK(rtype::voice::levelFraction(-40.0f) == Approx(0.5f));
    CHECK(rtype::voice::levelFraction(0.0f) == 1.0f);
    CHECK(rtype::voice::levelFraction(rtype::voice::SilenceFloor) == 0.0f);
    CHECK(rtype::voice::levelAt(0.5f) == Approx(-40.0f));
    CHECK(rtype::voice::levelAt(rtype::voice::levelFraction(-25.0f)) == Approx(-25.0f));
}
