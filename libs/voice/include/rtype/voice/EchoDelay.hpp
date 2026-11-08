#pragma once

#include "rtype/voice/Format.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

/// How late the echo of what the speakers play comes back in the microphone
/// (docs/architecture/voice.md#the-sound-processing). Pure: the samples are given, nothing is opened.
namespace rtype::voice
{

/// One measure of the delay of the echo.
struct EchoEstimate {
    /// How many samples after the reference its echo is in the microphone, at 48 kHz: negative when the
    /// microphone is ahead of the reference given (the reference came late).
    std::ptrdiff_t delay = 0;
    /// The normalized correlation of the microphone with the reference at that delay, from -1 to 1: 1 when
    /// the microphone is the reference, delayed and scaled.
    float correlation = 0.0f;
};

/// The samples a measure compares are decimated by this: 2 kHz, enough for the shape of a voice or a sound,
/// to half a millisecond.
inline constexpr std::size_t EchoDecimation = 24;
/// The reference a measure compares: 1 s.
inline constexpr std::size_t EchoWindow = static_cast<std::size_t>(SampleRate);
/// The delays a measure looks at, from EchoEarliest before the reference to EchoLatest after it.
inline constexpr std::size_t EchoEarliest = static_cast<std::size_t>(SampleRate) / 50;  // 20 ms
inline constexpr std::size_t EchoLatest = static_cast<std::size_t>(SampleRate) * 2 / 5; // 400 ms
/// The microphone a measure compares: the window, and every delay looked at.
inline constexpr std::size_t EchoMicrophoneWindow = EchoWindow + EchoEarliest + EchoLatest;

/// Measures the delay of the echo: the delay where the microphone looks most like the reference, by their
/// correlation, at 2 kHz.
///
/// @param reference EchoWindow samples of what was played.
/// @param microphone EchoMicrophoneWindow samples of the microphone, the first one heard EchoEarliest before
///        the first of the reference was played.
/// @return Nothing if either is silent, or not of these sizes.
///
/// @code
/// if (const auto echo = voice::measureEchoDelay(reference, microphone); echo && echo->correlation > 0.3f) {
///     // The echo comes echo->delay samples after what is played
/// }
/// @endcode
[[nodiscard]] std::optional<EchoEstimate> measureEchoDelay(std::span<const std::int16_t> reference,
                                                           std::span<const std::int16_t> microphone);

/// The delay of the echo, from one measure to the next: the first one that is likely kept at once, the next
/// ones only when two in a row agree, so that a measure fooled by the voice of the player, which is not in
/// the reference, does not move it.
class EchoDelay
{
    std::optional<std::ptrdiff_t> _delay;
    std::optional<std::ptrdiff_t> _candidate;

public:
    /// Under this correlation, a measure is not believed: the microphone hears mostly something else.
    static constexpr float MinCorrelation = 0.3f;
    /// Two measures this close, in samples, agree: 1 ms.
    static constexpr std::ptrdiff_t Agreement = SampleRate / 1000;

    /// A new measure.
    /// @return Whether the delay changed.
    bool update(const EchoEstimate &estimate);
    /// The delay kept, nothing before a first measure believed.
    [[nodiscard]] std::optional<std::ptrdiff_t> delay() const { return _delay; }
    void reset()
    {
        _delay.reset();
        _candidate.reset();
    }
};

} // namespace rtype::voice
