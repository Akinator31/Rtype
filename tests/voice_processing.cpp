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

using rtype::voice::FrameSamples;
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

/// What a game plays, as far as an echo canceller cares: noise in bursts of 100 to 400 ms, louder and
/// quieter, with pauses, the same for a seed
Samples bursts(std::size_t count, double deviation, unsigned seed)
{
    std::mt19937 random(seed);
    std::normal_distribution<double> normal(0.0, 1.0);
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    Samples samples(count);
    std::size_t at = 0;
    while (at < count) {
        const auto length = static_cast<std::size_t>((0.1 + (0.3 * uniform(random))) * SampleRate);
        const double level = uniform(random) < 0.2 ? 0.0 : deviation * (0.3 + uniform(random));
        for (std::size_t i = at; i < std::min(count, at + length); ++i) {
            const double fade = std::sin(std::numbers::pi * static_cast<double>(i - at) / length);
            samples[i] = clamp16(normal(random) * level * fade);
        }
        at += length;
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

/// The same room, the echo coming later from this sample on: the output or the input lost samples
Samples room(const Samples &played, std::size_t before, std::size_t after, std::size_t from)
{
    const Samples early = ::room(played, before);
    const Samples late = ::room(played, after);
    Samples heard(early.begin(), early.begin() + static_cast<std::ptrdiff_t>(from));
    heard.insert(heard.end(), late.begin() + static_cast<std::ptrdiff_t>(from), late.end());
    return heard;
}

/// How much quieter the output is than what the microphone heard, from one sample to another, in dB
double cutBetween(const Samples &heard, const Samples &out, std::size_t from, std::size_t to)
{
    return decibels(power(std::span(heard).subspan(from, to - from)) /
                    power(std::span(out).subspan(from, to - from)));
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

TEST_CASE("The echo of what the speakers play is cut by 20 dB at least, from the start", "[voice]")
{
    constexpr std::size_t seconds = 10;
    const Samples played = bursts(seconds * SampleRate, 4000.0, 3);
    for (const std::size_t delay : {std::size_t{20}, std::size_t{60}, std::size_t{250}}) {
        // That long after the sound is played (the sound played being given a frame ahead)
        const Samples heard = room(played, (delay * Ms) - FrameSamples);
        SoundProcessing processing({.noiseSuppression = false, .gainControl = false});
        const Samples out = run(processing, played, heard);
        REQUIRE(out.size() == heard.size());
        const double first = cutBetween(heard, out, 0, SampleRate);
        const double then = cutBetween(heard, out, SampleRate, out.size());
        INFO("Echo of " << delay << " ms cut by " << first << " dB in the first second, " << then
                        << " dB after it");
        // The usual delays from the first second: what the filter has not learnt yet is suppressed. A long
        // one, of a Bluetooth headset, once the canceller found it, within half a second
        if (delay < 100) {
            CHECK(first >= 20.0);
        }
        CHECK(then >= 40.0);
    }
}

TEST_CASE("An echo whose delay moves is cut again within two seconds", "[voice]")
{
    constexpr std::size_t seconds = 10;
    const Samples played = bursts(seconds * SampleRate, 4000.0, 8);
    // 60 ms, then 140 ms from the fifth second: the input lost 80 ms. The canceller finds the new delay in 1
    // to 1.5 s, the echo cut partly meanwhile
    const Samples heard = room(played, (60 * Ms) - FrameSamples, (140 * Ms) - FrameSamples, 5 * SampleRate);
    SoundProcessing processing({.noiseSuppression = false, .gainControl = false});
    const Samples out = run(processing, played, heard);
    const double before = cutBetween(heard, out, 2 * SampleRate, 5 * SampleRate);
    const double after = cutBetween(heard, out, 7 * SampleRate, seconds * SampleRate);
    INFO("Echo cut by " << before << " dB before it moved, " << after << " dB two seconds after");
    CHECK(before >= 30.0);
    CHECK(after >= 30.0);
}

TEST_CASE("The voice of the player is kept while the echo is cut", "[voice]")
{
    constexpr std::size_t seconds = 10;
    const Samples played = bursts(seconds * SampleRate, 3000.0, 4);
    const Samples echo = room(played, (50 * Ms) - FrameSamples);
    const Samples spoken = voice(seconds * SampleRate, 3000.0);
    SoundProcessing processing({.noiseSuppression = false, .gainControl = false});
    const Samples out = run(processing, played, mix(echo, spoken));

    // The last 2 s: the voice comes out near its level, the echo under it
    const std::size_t from = out.size() - (2 * SampleRate);
    const double kept =
        decibels(power(std::span(out).subspan(from)) / power(std::span(spoken).subspan(from)));
    INFO("Voice kept at " << kept << " dB");
    CHECK(std::abs(kept) <= 6.0);
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

    // The chain delays the voice by a few milliseconds: taken 10 ms later, past the first second, which the
    // suppressor learns from, the middle of each syllable (100 to 150 ms of its 250), and of each pause
    // (300 to 350 ms)
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

TEST_CASE("The gain control brings a quiet voice up, and a loud one under clipping once it has adapted",
          "[voice]")
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
        // Past the first second, where the gain it starts from is brought down to the voice
        const auto loudest = std::ranges::max(std::span(out).subspan(SampleRate), {},
                                              [](std::int16_t sample) { return std::abs(sample); });
        CHECK(std::abs(loudest) < 32000);
    }
}

TEST_CASE("Frames dropped by a game loop that stalled leave the echo cut", "[voice]")
{
    const Samples played = bursts(8 * SampleRate, 3000.0, 6);
    const Samples heard = room(played, (60 * Ms) - FrameSamples);
    SoundProcessing processing({.noiseSuppression = false, .gainControl = false});
    Samples in;
    Samples out;
    processing.played(std::span(played).first(FrameSamples));
    for (std::size_t at = 0; at + FrameSamples <= heard.size(); at += FrameSamples) {
        if (at + (2 * FrameSamples) <= played.size()) {
            processing.played(std::span(played).subspan(at + FrameSamples, FrameSamples));
        }
        processing.captured(std::span(heard).subspan(at, FrameSamples));
        // At the fourth second, a stall: 5 frames of the microphone dropped
        if (at / FrameSamples == 4 * rtype::voice::FramesPerSecond) {
            processing.played(std::span(played).subspan(at + (2 * FrameSamples), 5 * FrameSamples));
            processing.captured(std::span(heard).subspan(at + FrameSamples, 5 * FrameSamples));
            processing.drop(5);
            at += 5 * FrameSamples;
        }
        Frame frame{};
        while (processing.next(frame)) {
            out.insert(out.end(), frame.begin(), frame.end());
            in.insert(in.end(), processing.heard().begin(), processing.heard().end());
        }
    }
    CHECK(processing.waiting() == 0);
    const double cut = cutBetween(in, out, out.size() - (2 * SampleRate), out.size());
    INFO("Echo cut by " << cut << " dB after the stall");
    CHECK(cut >= 40.0);
}

TEST_CASE("The microphone waits a second at most for next()", "[voice]")
{
    SoundProcessing processing;
    const Samples spoken = voice(3 * SampleRate, 3000.0);
    processing.captured(spoken);
    CHECK(processing.waiting() == SoundProcessing::MaxWaiting / FrameSamples);
    processing.drop(2);
    CHECK(processing.waiting() == (SoundProcessing::MaxWaiting / FrameSamples) - 2);
    processing.reset();
    CHECK(processing.waiting() == 0);
}

TEST_CASE("With nothing played and the processing off, the microphone goes out as it came", "[voice]")
{
    SoundProcessing processing({.noiseSuppression = false, .gainControl = false});
    const Samples spoken = voice(SampleRate / 2, 3000.0);
    processing.captured(spoken);
    CHECK(processing.waiting() == spoken.size() / FrameSamples);
    Samples out;
    Frame frame{};
    while (processing.next(frame)) {
        CHECK(std::ranges::equal(frame, processing.heard()));
        out.insert(out.end(), frame.begin(), frame.end());
    }
    CHECK(out == Samples(spoken.begin(), spoken.begin() + static_cast<std::ptrdiff_t>(out.size())));
    CHECK(out.size() == (spoken.size() / FrameSamples) * FrameSamples);
}
