#pragma once

#include "rtype/voice/Format.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

/// What the microphone goes through before the encoder (docs/architecture/voice.md#the-sound-processing):
/// the echo cancellation, the noise suppression and the gain control. speexdsp and RNNoise stay out of this
/// header.
namespace rtype::voice
{

/// What the processing does, from the Voice page. The echo cancellation has no setting: it runs whenever
/// something is played.
struct ProcessingSettings {
    /// The noises of the room (a fan, keys, a breath) taken out by RNNoise.
    bool noiseSuppression = true;
    /// The level of the voice brought to the same, near or far from the microphone, by the AGC of speexdsp.
    bool gainControl = true;

    friend bool operator==(const ProcessingSettings &, const ProcessingSettings &) = default;
};

/// The chain of the microphone, frame of 20 ms by frame, in two halves of 10 ms:
///
/// 1. **Echo cancellation**, by speex_echo: the reference is what the speakers played (Loopback), at the
///    delay measured between it and the microphone (EchoDelay), so that the 150 ms its filter covers start
///    just before the echo.
/// 2. **Noise suppression**, by RNNoise, if on.
/// 3. **What is left of the echo** taken out, and the **gain control** if on, by speex_preprocess.
///
/// The microphone and the reference are two streams of samples counted from open: the reference of a sample
/// of the microphone is found by their counts, from the first call where both have come. Either losing
/// samples (a game loop stalled for a second) moves one against the other; it is seen, and both start
/// again.
///
/// @code
/// voice::SoundProcessing processing;
/// processing.played(loopbackSamples);      // what the speakers played since the last time
/// processing.captured(microphoneSamples);  // what the microphone heard since the last time
/// std::array<std::int16_t, FrameSamples> frame;
/// while (processing.next(frame)) {
///     encoder.encode(frame);
/// }
/// @endcode
///
/// Every call is made on the thread of the game loop, never in an audio callback.
class SoundProcessing
{
    struct State;
    std::unique_ptr<State> _state;

public:
    /// The samples speexdsp and RNNoise take at once: 10 ms.
    static constexpr std::size_t HalfFrame = FrameSamples / 2;
    /// How long an echo the filter of the canceller follows, from its start: 150 ms.
    static constexpr std::size_t EchoTail = static_cast<std::size_t>(SampleRate) * 3 / 20;
    /// Where the filter starts, before the echo measured: 5 ms, for the measure to be a little late.
    static constexpr std::size_t EchoMargin = static_cast<std::size_t>(SampleRate) / 200;
    /// Where the echo is taken to start before it is measured: 20 ms, about the least an output and an input
    /// add, so that the filter covers echoes of 20 to 170 ms.
    static constexpr std::size_t EchoAssumed = static_cast<std::size_t>(SampleRate) / 50;
    /// The microphone and the reference moved by more than this, in samples, against each other since they
    /// were paired: one of them lost samples, both start again. 80 ms: more than the periods of two devices.
    static constexpr std::size_t MaxDrift = static_cast<std::size_t>(SampleRate) * 2 / 25;
    /// How often the delay of the echo is measured again, in samples of the microphone: 2 s.
    static constexpr std::size_t MeasureEvery = 2 * static_cast<std::size_t>(SampleRate);

    explicit SoundProcessing(const ProcessingSettings &settings = {});
    ~SoundProcessing();

    SoundProcessing(const SoundProcessing &) = delete;
    SoundProcessing &operator=(const SoundProcessing &) = delete;

    void setSettings(const ProcessingSettings &settings);
    [[nodiscard]] const ProcessingSettings &settings() const;

    /// Forgets everything: the samples waiting, the echo learnt and its delay. For a microphone opened again.
    void reset();
    /// What the speakers played since the last call, in order.
    void played(std::span<const std::int16_t> samples);
    /// What the microphone heard since the last call, in order.
    void captured(std::span<const std::int16_t> samples);
    /// The frames of the microphone waiting for next().
    [[nodiscard]] std::size_t waiting() const;
    /// Forgets the oldest frames waiting, of a game loop that stalled, so that they are not sent late. The
    /// pairing of the two streams is kept.
    void drop(std::size_t frames);
    /// Processes the oldest frame waiting.
    /// @return false if there is none.
    bool next(std::span<std::int16_t, FrameSamples> frame);
    /// The last frame next() processed, as the microphone heard it: to tell a microphone that gives only
    /// silence, which the noise suppression would give too.
    [[nodiscard]] std::span<const std::int16_t, FrameSamples> heard() const;

    /// The delay of the echo measured, in samples at 48 kHz, from the reference to the microphone as they
    /// were paired: nothing before a first measure, or without a reference.
    [[nodiscard]] std::optional<std::ptrdiff_t> echoDelay() const;
    /// The streams were paired again since reset(), one of them having lost samples.
    [[nodiscard]] std::size_t repairs() const;
};

} // namespace rtype::voice
