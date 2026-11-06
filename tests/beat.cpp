// Tests of the detection of the beats of a music (libs/beat).

#include "rtype/beat/Analysis/BeatDetector.hpp"
#include "rtype/beat/Decoder/Mp3Decoder.hpp"
#include "rtype/beat/Decoder/WavDecoder.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numbers>
#include <stdexcept>

using namespace rtype::beat;
using Catch::Approx;

namespace
{

constexpr std::uint32_t SampleRate = 44100;
const BeatGaps NoGap{0.0f, 0.0f, 0.0f};

/// Adds a short burst of a sine of `hertz` at `second`.
void addBurst(MonoSound &sound, float second, float hertz, float length = 0.05f)
{
    const auto start = static_cast<std::size_t>(second * static_cast<float>(SampleRate));
    const auto count = static_cast<std::size_t>(length * static_cast<float>(SampleRate));
    for (std::size_t i = 0; i < count && start + i < sound.samples.size(); ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(SampleRate);
        const float fade = std::min(1.0f, static_cast<float>(i) / 220.0f) * (1.0f - static_cast<float>(i) / static_cast<float>(count));
        sound.samples[start + i] += 0.8f * fade * std::sin(2.0f * std::numbers::pi_v<float> * hertz * t);
    }
}

MonoSound silence(float seconds)
{
    MonoSound sound;
    sound.sampleRate = SampleRate;
    sound.samples.assign(static_cast<std::size_t>(seconds * static_cast<float>(SampleRate)), 0.0f);
    return sound;
}

} // namespace

TEST_CASE("Silence has no beat", "[beat]")
{
    CHECK(detectBeats(silence(3.0f), NoGap).empty());
}

TEST_CASE("A sound shorter than a frame has no beat", "[beat]")
{
    CHECK(detectBeats(silence(0.01f), NoGap).empty());
}

TEST_CASE("Bursts are found in the band of their frequency, at their time", "[beat]")
{
    MonoSound sound = silence(6.0f);
    addBurst(sound, 1.0f, 80.0f);
    addBurst(sound, 2.0f, 800.0f);
    addBurst(sound, 3.0f, 5000.0f);
    addBurst(sound, 4.0f, 80.0f);

    const std::vector<Beat> beats = detectBeats(sound, NoGap);

    REQUIRE(beats.size() == 4);
    CHECK(beats[0].band == BeatBand::Bass);
    CHECK(beats[1].band == BeatBand::Mid);
    CHECK(beats[2].band == BeatBand::High);
    CHECK(beats[3].band == BeatBand::Bass);
    // The window of the analysis is centered on the frame: the time is within a frame of the burst
    CHECK(beats[0].time == Approx(1.0f).margin(0.08f));
    CHECK(beats[1].time == Approx(2.0f).margin(0.08f));
    CHECK(beats[3].time == Approx(4.0f).margin(0.08f));
}

TEST_CASE("Beats of a band closer than its gap are merged", "[beat]")
{
    MonoSound sound = silence(4.0f);
    addBurst(sound, 1.0f, 80.0f);
    addBurst(sound, 1.2f, 80.0f);
    addBurst(sound, 2.5f, 80.0f);

    const std::vector<Beat> beats = detectBeats(sound, BeatGaps{0.5f, 0.5f, 0.5f});

    CHECK(detectBeats(sound, NoGap).size() >= beats.size());
    for (std::size_t i = 0; i < beats.size(); ++i) {
        for (std::size_t j = i + 1; j < beats.size(); ++j) {
            if (beats[i].band == beats[j].band) {
                CHECK(beats[j].time - beats[i].time >= 0.5f);
            }
        }
    }
}

TEST_CASE("The beats come by increasing time", "[beat]")
{
    MonoSound sound = silence(4.0f);
    addBurst(sound, 2.0f, 80.0f);
    addBurst(sound, 1.0f, 5000.0f);

    const std::vector<Beat> beats = detectBeats(sound, NoGap);

    REQUIRE(beats.size() == 2);
    CHECK(beats[0].time < beats[1].time);
}

TEST_CASE("A missing or invalid mp3 is refused", "[beat]")
{
    CHECK_THROWS_AS(decodeMp3("no/such/file.mp3"), std::runtime_error);
    CHECK_THROWS_AS(decodeMp3(std::filesystem::path(RTYPE_TEST_CONFIG) / "player.lua"), std::runtime_error);
}

TEST_CASE("A wav is decoded, and another extension is refused", "[beat]")
{
    const std::filesystem::path sounds(RTYPE_TEST_SOUNDS);

    const MonoSound sound = decodeSound(sounds / "punk.wav");

    CHECK(sound.sampleRate == 44100);
    CHECK(sound.duration() > 30.0f);
    CHECK_THROWS_AS(decodeWav("no/such/file.wav"), std::runtime_error);
    CHECK_THROWS_AS(decodeSound(std::filesystem::path(RTYPE_TEST_CONFIG) / "player.lua"), std::runtime_error);
}

TEST_CASE("The music of a wave is decoded and has beats in each band", "[beat]")
{
    const MonoSound sound = decodeMp3(std::filesystem::path(RTYPE_TEST_SOUNDS) / "drums_style.mp3");

    CHECK(sound.sampleRate > 0);
    CHECK(sound.duration() > 30.0f);
    const std::vector<Beat> beats = detectBeats(sound, BeatGaps{0.25f, 0.25f, 0.25f});
    std::array<int, BeatBandCount> perBand{};
    for (const Beat &beat : beats) {
        ++perBand[static_cast<std::size_t>(beat.band)];
    }
    CHECK(perBand[0] > 20);
    CHECK(perBand[1] > 20);
    CHECK(perBand[2] > 20);
}
