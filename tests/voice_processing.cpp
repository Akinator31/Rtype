#include "rtype/voice/EchoDelay.hpp"
#include "rtype/voice/Processing.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <random>
#include <span>
#include <vector>

using rtype::voice::EchoDelay;
using rtype::voice::EchoEarliest;
using rtype::voice::EchoEstimate;
using rtype::voice::EchoMicrophoneWindow;
using rtype::voice::EchoWindow;
using rtype::voice::FrameSamples;
using rtype::voice::measureEchoDelay;
using rtype::voice::SampleRate;
using rtype::voice::SoundProcessing;

namespace
{

using Samples = std::vector<std::int16_t>;
using Frame = std::array<std::int16_t, FrameSamples>;

constexpr std::size_t Ms = static_cast<std::size_t>(SampleRate) / 1000;

std::int16_t clamp16(double value)
{
    return static_cast<std::int16_t>(std::clamp(std::round(value), -32768.0, 32767.0));
}

/// Noise of this deviation, the same for a seed: what a game plays, as far as an echo canceller cares
Samples noise(std::size_t count, double deviation, unsigned seed)
{
    std::mt19937 random(seed);
    std::normal_distribution<double> normal(0.0, deviation);
    Samples samples(count);
    for (std::int16_t &sample : samples) {
        sample = clamp16(normal(random));
    }
    return samples;
}

/// A voice-like sound: a pitch of 180 Hz with harmonics falling as a voice's do, spoken in syllables of
/// 250 ms with pauses of 150 ms between them
Samples voice(std::size_t count, double amplitude)
{
    Samples samples(count);
    double phase = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / SampleRate;
        phase += 2.0 * std::numbers::pi * (180.0 + (20.0 * std::sin(2.0 * std::numbers::pi * 3.0 * t))) /
                 SampleRate;
        const double inSyllable = std::fmod(t, 0.4);
        const double envelope = inSyllable < 0.25 ? std::sin(std::numbers::pi * inSyllable / 0.25) : 0.0;
        double value = 0.0;
        for (int harmonic = 1; harmonic <= 18; ++harmonic) {
            value += std::sin(harmonic * phase) / harmonic;
        }
        samples[i] = clamp16(value * amplitude * envelope);
    }
    return samples;
}

/// What a microphone hears of a sound played in a room: the sound late by this delay, and its reflections
Samples room(const Samples &played, std::size_t delay)
{
    struct Tap {
        std::size_t at;
        double gain;
    };
    constexpr std::array<Tap, 4> taps{{{0, 0.5}, {3 * Ms, 0.25}, {15 * Ms, -0.15}, {40 * Ms, 0.05}}};
    Samples heard(played.size());
    for (std::size_t i = 0; i < heard.size(); ++i) {
        double value = 0.0;
        for (const Tap &tap : taps) {
            if (i >= delay + tap.at) {
                value += tap.gain * played[i - delay - tap.at];
            }
        }
        heard[i] = clamp16(value);
    }
    return heard;
}

Samples mix(const Samples &a, const Samples &b)
{
    Samples sum(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        sum[i] = clamp16(static_cast<double>(a[i]) + b[i]);
    }
    return sum;
}

double power(std::span<const std::int16_t> samples)
{
    double sum = 0.0;
    for (const std::int16_t sample : samples) {
        sum += static_cast<double>(sample) * sample;
    }
    return samples.empty() ? 0.0 : sum / static_cast<double>(samples.size());
}

double decibels(double ratio)
{
    return 10.0 * std::log10(std::max(ratio, 1e-12));
}

/// The power of samples at one frequency, by Goertzel
double powerAt(std::span<const std::int16_t> samples, double frequency)
{
    const double coefficient = 2.0 * std::cos(2.0 * std::numbers::pi * frequency / SampleRate);
    double previous = 0.0;
    double before = 0.0;
    for (const std::int16_t sample : samples) {
        const double current = sample + (coefficient * previous) - before;
        before = previous;
        previous = current;
    }
    return ((previous * previous) + (before * before) - (coefficient * previous * before)) /
           static_cast<double>(samples.size() * samples.size());
}

/// A microphone and the sound played, given to a SoundProcessing as a game loop would: every 20 ms, what
/// each stream gave since the last time, the sound played a frame ahead of the microphone
Samples run(SoundProcessing &processing, const Samples &played, const Samples &heard)
{
    Samples out;
    processing.played(std::span(played).first(FrameSamples));
    for (std::size_t at = 0; at + FrameSamples <= heard.size(); at += FrameSamples) {
        if (at + (2 * FrameSamples) <= played.size()) {
            processing.played(std::span(played).subspan(at + FrameSamples, FrameSamples));
        }
        processing.captured(std::span(heard).subspan(at, FrameSamples));
        Frame frame{};
        while (processing.next(frame)) {
            out.insert(out.end(), frame.begin(), frame.end());
        }
    }
    return out;
}

} // namespace

TEST_CASE("The delay of an echo is measured from the sound played and the microphone", "[voice]")
{
    const Samples played = noise(EchoMicrophoneWindow + EchoWindow, 3000.0, 1);
    for (const std::size_t delay : {std::size_t{0}, 7 * Ms, 60 * Ms, 230 * Ms, 390 * Ms}) {
        const Samples heard = room(played, delay);
        // The window of the microphone starts EchoEarliest before the one of the reference
        const std::size_t from = EchoWindow;
        const std::optional<EchoEstimate> echo =
            measureEchoDelay(std::span(played).subspan(from, EchoWindow),
                             std::span(heard).subspan(from - EchoEarliest, EchoMicrophoneWindow));
        REQUIRE(echo);
        CHECK(std::abs(echo->delay - static_cast<std::ptrdiff_t>(delay)) <= static_cast<std::ptrdiff_t>(Ms));
        CHECK(echo->correlation > 0.5f);
    }
}

TEST_CASE("No delay is measured without a sound played, nor from windows of other sizes", "[voice]")
{
    const Samples silence(EchoMicrophoneWindow, 0);
    const Samples heard = noise(EchoMicrophoneWindow, 3000.0, 2);
    CHECK_FALSE(measureEchoDelay(std::span(silence).first(EchoWindow), heard));
    CHECK_FALSE(measureEchoDelay(std::span(heard).first(EchoWindow - 1), heard));
}

TEST_CASE("The delay kept moves only when two measures agree", "[voice]")
{
    EchoDelay delay;
    CHECK_FALSE(delay.update({.delay = 2400, .correlation = 0.1f}));
    CHECK_FALSE(delay.delay());
    CHECK(delay.update({.delay = 2400, .correlation = 0.6f}));
    CHECK(delay.delay() == 2400);
    // One measure fooled by the voice of the player: kept aside
    CHECK_FALSE(delay.update({.delay = 9000, .correlation = 0.4f}));
    CHECK(delay.delay() == 2400);
    CHECK_FALSE(delay.update({.delay = 2410, .correlation = 0.6f}));
    CHECK_FALSE(delay.update({.delay = 3000, .correlation = 0.5f}));
    CHECK(delay.update({.delay = 3020, .correlation = 0.5f}));
    CHECK(delay.delay() == 3020);
}

TEST_CASE("The echo of what the speakers play is cut by 20 dB at least", "[voice]")
{
    constexpr std::size_t seconds = 10;
    const Samples played = noise(seconds * SampleRate, 4000.0, 3);
    // 250 ms after the sound is played, as the two streams are paired (the sound played being a frame ahead):
    // past the 20 to 170 ms the filter covers before a measure, so that the delay measured is needed
    const Samples heard = room(played, (250 * Ms) - FrameSamples);
    SoundProcessing processing({.noiseSuppression = false, .gainControl = false});
    const Samples out = run(processing, played, heard);
    REQUIRE(out.size() == heard.size());

    REQUIRE(processing.echoDelay());
    CHECK(std::abs(*processing.echoDelay() - static_cast<std::ptrdiff_t>(250 * Ms)) <=
          static_cast<std::ptrdiff_t>(Ms));
    // The last 3 s, the filter having learnt the room
    const std::size_t from = out.size() - (3 * SampleRate);
    const double cut = decibels(power(std::span(heard).subspan(from)) / power(std::span(out).subspan(from)));
    INFO("Echo cut by " << cut << " dB");
    CHECK(cut >= 20.0);
    CHECK(processing.repairs() == 0);
}

TEST_CASE("The voice of the player is kept while the echo is cut", "[voice]")
{
    constexpr std::size_t seconds = 10;
    const Samples played = noise(seconds * SampleRate, 3000.0, 4);
    const Samples echo = room(played, (50 * Ms) - FrameSamples);
    const Samples spoken = voice(seconds * SampleRate, 3000.0);
    SoundProcessing processing({.noiseSuppression = false, .gainControl = false});
    const Samples out = run(processing, played, mix(echo, spoken));

    // The last 2 s: the voice comes out at its level, the echo under it
    const std::size_t from = out.size() - (2 * SampleRate);
    const double kept =
        decibels(power(std::span(out).subspan(from)) / power(std::span(spoken).subspan(from)));
    INFO("Voice kept at " << kept << " dB");
    CHECK(std::abs(kept) <= 3.0);
}

TEST_CASE("The noise of the room is taken out and the voice kept, its level and spectrum", "[voice]")
{
    constexpr std::size_t seconds = 4;
    const Samples spoken = voice(seconds * SampleRate, 3000.0);
    const Samples room = noise(seconds * SampleRate, 150.0, 5);
    const Samples heard = mix(spoken, room);
    SoundProcessing processing({.noiseSuppression = true, .gainControl = false});
    Samples out;
    for (std::size_t at = 0; at + FrameSamples <= heard.size(); at += FrameSamples) {
        processing.captured(std::span(heard).subspan(at, FrameSamples));
        Frame frame{};
        while (processing.next(frame)) {
            out.insert(out.end(), frame.begin(), frame.end());
        }
    }
    REQUIRE(out.size() == heard.size());

    // The output of RNNoise is 10 ms late. Past the first second, which it learns from: the middle of each
    // syllable (100 to 150 ms of its 250), and the middle of each pause (300 to 350 ms)
    const std::size_t late = 10 * Ms;
    double voiceIn = 0.0;
    double voiceOut = 0.0;
    double pauseIn = 0.0;
    double pauseOut = 0.0;
    std::vector<double> harmonicsIn(16, 0.0);
    std::vector<double> harmonicsOut(16, 0.0);
    for (std::size_t syllable = 400 * Ms; syllable + (400 * Ms) <= out.size() - late; syllable += 400 * Ms) {
        const auto in = std::span(heard).subspan(syllable + (100 * Ms), 50 * Ms);
        const auto outVoice = std::span(out).subspan(syllable + (100 * Ms) + late, 50 * Ms);
        voiceIn += power(in);
        voiceOut += power(outVoice);
        pauseIn += power(std::span(heard).subspan(syllable + (300 * Ms), 50 * Ms));
        pauseOut += power(std::span(out).subspan(syllable + (300 * Ms) + late, 50 * Ms));
        for (std::size_t harmonic = 0; harmonic < harmonicsIn.size(); ++harmonic) {
            const double frequency = 180.0 * static_cast<double>(harmonic + 1);
            harmonicsIn[harmonic] += powerAt(in, frequency);
            harmonicsOut[harmonic] += powerAt(outVoice, frequency);
        }
    }
    const double level = decibels(voiceOut / voiceIn);
    const double quieter = decibels(pauseIn / pauseOut);
    INFO("Voice at " << level << " dB, noise cut by " << quieter << " dB");
    CHECK(std::abs(level) <= 6.0);
    CHECK(quieter >= 10.0);
    // Each harmonic of the voice up to 2.9 kHz within 6 dB of what it was
    for (std::size_t harmonic = 0; harmonic < harmonicsIn.size(); ++harmonic) {
        const double change = decibels(harmonicsOut[harmonic] / harmonicsIn[harmonic]);
        INFO("Harmonic " << harmonic + 1 << " moved by " << change << " dB");
        CHECK(std::abs(change) <= 6.0);
    }
}

TEST_CASE("The gain control brings a quiet voice up, and a loud one stays under clipping", "[voice]")
{
    constexpr std::size_t seconds = 8;
    for (const double amplitude : {300.0, 12000.0}) {
        const Samples spoken = voice(seconds * SampleRate, amplitude);
        SoundProcessing processing({.noiseSuppression = false, .gainControl = true});
        Samples out;
        for (std::size_t at = 0; at + FrameSamples <= spoken.size(); at += FrameSamples) {
            processing.captured(std::span(spoken).subspan(at, FrameSamples));
            Frame frame{};
            while (processing.next(frame)) {
                out.insert(out.end(), frame.begin(), frame.end());
            }
        }
        const std::size_t from = out.size() - (2 * SampleRate);
        const double change =
            decibels(power(std::span(out).subspan(from)) / power(std::span(spoken).subspan(from)));
        INFO("Amplitude " << amplitude << ": changed by " << change << " dB");
        if (amplitude < 1000.0) {
            CHECK(change >= 6.0);
        }
        const auto loudest = std::ranges::max(out, {}, [](std::int16_t sample) { return std::abs(sample); });
        CHECK(std::abs(loudest) < 32000);
    }
}

TEST_CASE("Frames dropped keep the microphone paired with the sound played", "[voice]")
{
    const Samples played = noise(4 * SampleRate, 3000.0, 6);
    const Samples heard = room(played, (60 * Ms) - FrameSamples);
    SoundProcessing processing({.noiseSuppression = false, .gainControl = false});
    processing.played(std::span(played).first(FrameSamples));
    for (std::size_t at = 0; at + FrameSamples <= heard.size(); at += FrameSamples) {
        if (at + (2 * FrameSamples) <= played.size()) {
            processing.played(std::span(played).subspan(at + FrameSamples, FrameSamples));
        }
        processing.captured(std::span(heard).subspan(at, FrameSamples));
        // A game loop that keeps one frame in four
        processing.drop(processing.waiting() > 0 && (at / FrameSamples) % 4 != 0 ? 1 : 0);
        Frame frame{};
        while (processing.next(frame)) {
        }
    }
    CHECK(processing.waiting() == 0);
    CHECK(processing.repairs() == 0);
}

TEST_CASE("A microphone that loses samples is paired again with the sound played", "[voice]")
{
    const Samples played = noise(3 * SampleRate, 3000.0, 7);
    SoundProcessing processing;
    processing.played(std::span(played).first(SampleRate));
    processing.captured(std::span(played).first(FrameSamples));
    CHECK(processing.repairs() == 0);
    // A second played, 20 ms heard: the microphone lost the rest
    processing.played(std::span(played).subspan(SampleRate, SampleRate));
    processing.captured(std::span(played).subspan(FrameSamples, FrameSamples));
    CHECK(processing.repairs() == 1);
    // Steady again
    processing.played(std::span(played).subspan(2 * SampleRate, FrameSamples));
    processing.captured(std::span(played).subspan(2 * FrameSamples, FrameSamples));
    CHECK(processing.repairs() == 1);
    processing.reset();
    CHECK(processing.repairs() == 0);
    CHECK(processing.waiting() == 0);
}

TEST_CASE("Without a sound played, the microphone goes through the chain alone", "[voice]")
{
    SoundProcessing processing({.noiseSuppression = false, .gainControl = false});
    const Samples spoken = voice(SampleRate, 3000.0);
    processing.captured(spoken);
    CHECK(processing.waiting() == spoken.size() / FrameSamples);
    Frame frame{};
    std::size_t frames = 0;
    while (processing.next(frame)) {
        ++frames;
    }
    CHECK(frames == spoken.size() / FrameSamples);
    CHECK_FALSE(processing.echoDelay());
}
