#pragma once

#include "rtype/voice/Format.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

/// What the microphone goes through before the encoder (docs/architecture/voice.md#the-sound-processing):
/// the echo cancellation, the noise suppression and the gain control of WebRTC. WebRTC stays out of this
/// header.
namespace rtype::voice
{

/// What the processing does, from the Voice page. The echo cancellation has no setting: it runs whenever
/// something is played.
struct ProcessingSettings {
    /// The noises of the room (a fan, a hum, a breath) taken out.
    bool noiseSuppression = true;
    /// The level of the voice brought to the same, near or far from the microphone.
    bool gainControl = true;

    friend bool operator==(const ProcessingSettings &, const ProcessingSettings &) = default;
};

/// The chain of the microphone, the audio processing of WebRTC (webrtc-audio-processing), frame of 20 ms by
/// frame, in two halves of 10 ms:
///
/// 1. A **high-pass filter**, which takes the hum and the rumble under 80 Hz out.
/// 2. **Echo cancellation**, by AEC3: the reference is what the speakers played (Loopback). It finds the
///    delay of the echo itself, follows it when it moves, and suppresses what its filter leaves while it
///    learns.
/// 3. **Noise suppression**, if on.
/// 4. **Gain control**, AGC2 adaptive digital, if on, with a limiter so that a loud voice does not clip.
///
/// With nothing played, the noise suppression and the gain control off, the samples go out as they came.
///
/// @code
/// voice::SoundProcessing processing;
/// processing.played(loopbackSamples);      // what the speakers played since the last time, first
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
    /// The samples WebRTC takes at once: 10 ms.
    static constexpr std::size_t HalfFrame = FrameSamples / 2;
    /// The most samples of the microphone kept waiting for next(): 1 s. Older ones are dropped.
    static constexpr std::size_t MaxWaiting = static_cast<std::size_t>(SampleRate);

    explicit SoundProcessing(const ProcessingSettings &settings = {});
    ~SoundProcessing();

    SoundProcessing(const SoundProcessing &) = delete;
    SoundProcessing &operator=(const SoundProcessing &) = delete;

    void setSettings(const ProcessingSettings &settings);
    [[nodiscard]] const ProcessingSettings &settings() const;

    /// Forgets everything: the samples waiting and the echo learnt. For a microphone opened again.
    void reset();
    /// What the speakers played since the last call, in order: given to the echo canceller at once, before
    /// the microphone that heard it.
    void played(std::span<const std::int16_t> samples);
    /// What the microphone heard since the last call, in order.
    void captured(std::span<const std::int16_t> samples);
    /// The frames of the microphone waiting for next().
    [[nodiscard]] std::size_t waiting() const;
    /// Forgets the oldest frames waiting, of a game loop that stalled, so that they are not sent late. The
    /// echo canceller finds the delay again.
    void drop(std::size_t frames);
    /// Processes the oldest frame waiting.
    /// @return false if there is none.
    bool next(std::span<std::int16_t, FrameSamples> frame);
    /// The last frame next() processed, as the microphone heard it: to tell a microphone that gives only
    /// silence, which the noise suppression would give too.
    [[nodiscard]] std::span<const std::int16_t, FrameSamples> heard() const;
};

} // namespace rtype::voice
