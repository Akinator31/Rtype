#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

/// How the player talks (docs/architecture/voice.md#the-talk-modes): the mode chosen in the settings, the
/// push-to-talk key and the detection of the voice. Pure: the client gives the key and the time, the
/// GameVoice the samples.
namespace rtype::voice
{

using TalkClock = std::chrono::steady_clock;

/// When the microphone of the player is heard.
enum class TalkMode : std::uint8_t {
    /// While the push-to-talk key is held, and a moment after: the noise of the room stays out unasked.
    PushToTalk,
    /// While the level of the microphone is over the threshold of the settings.
    VoiceActivity,
    /// Never: the microphone stays closed.
    Off,
};

/// The modes in the order the settings go through them.
inline constexpr std::array<TalkMode, 3> TalkModes{TalkMode::PushToTalk, TalkMode::VoiceActivity,
                                                   TalkMode::Off};

/// How a mode is written in the settings ("push_to_talk"), and the end of the key of its name in the
/// translations ("voice.mode.push_to_talk").
[[nodiscard]] std::string_view name(TalkMode mode);
/// The mode written so, or nothing for a name that is not one.
[[nodiscard]] std::optional<TalkMode> talkModeFromName(std::string_view name);

/// The level of samples in decibels below the loudest sample (dBFS), from their mean power: 0 for a square at
/// full scale, about -3 for a sine at full scale. SilenceFloor for silence, and nothing below.
[[nodiscard]] float levelOf(std::span<const std::int16_t> samples);

/// The lowest level levelOf() gives: digital silence.
inline constexpr float SilenceFloor = -100.0f;
/// The thresholds of the voice activity the settings offer, and the one of a first run, in dBFS. -70 is
/// the breath of a quiet room in a headset, -10 a shout.
inline constexpr float MinThreshold = -70.0f;
inline constexpr float MaxThreshold = -10.0f;
inline constexpr float DefaultThreshold = -40.0f;

/// The push-to-talk key: the player talks while it is held, and a moment after it is released, so that the
/// end of the last word is not cut.
///
/// @code
/// pushToTalk.update(controls.isDown(PushToTalkControl), now);   // each frame
/// voice.setTalking(pushToTalk.talking(now));
/// @endcode
class PushToTalk
{
    bool _held = false;
    std::optional<TalkClock::time_point> _released;

public:
    /// How long the player still talks after the key is released.
    static constexpr std::chrono::milliseconds ReleaseDelay{200};

    /// Whether the key is held now.
    void update(bool held, TalkClock::time_point now);
    /// Whether the player talks now: the key held, or released less than ReleaseDelay ago.
    [[nodiscard]] bool talking(TalkClock::time_point now) const;
};

/// The detection of the voice, frame by frame: the player talks while the level of its frames is over the
/// threshold, and for VoiceActivity::Hold after the last one, so that the quiet ends of words and the pauses
/// between them are heard.
class VoiceActivity
{
    /// Frames still heard after the last one over the threshold.
    std::size_t _hold = 0;

public:
    /// How many frames of 20 ms the voice is still heard after the last one over the threshold: 300 ms.
    static constexpr std::size_t Hold = 15;

    /// A frame of the microphone, of this level, with the threshold of the settings.
    /// @return Whether the frame is part of the voice, to be sent.
    bool update(float level, float threshold);
    /// Forgets the voice heard: the next frame under the threshold is not sent.
    void reset() { _hold = 0; }
};

/// The level meter of the settings: the level of the frames of the microphone, which rises at once and falls
/// at 60 dB per second, so that the eye can follow it.
class LevelMeter
{
    float _level = SilenceFloor;
    /// Samples pushed that do not make a whole frame yet: their power, and their count.
    double _power = 0.0;
    std::size_t _count = 0;

public:
    /// How much the meter falls per frame of 20 ms.
    static constexpr float FallPerFrame = 1.2f;

    /// Samples of the microphone, as many as there are: the meter moves once per frame of 20 ms they hold.
    void push(std::span<const std::int16_t> samples);
    /// The level shown, in dBFS.
    [[nodiscard]] float level() const { return _level; }
    void reset()
    {
        _level = SilenceFloor;
        _power = 0.0;
        _count = 0;
    }
};

/// Where a level is between two others, from 0 to 1: the place of a level on the meter, and of the threshold
/// on its slider, which share the same scale.
[[nodiscard]] float levelFraction(float level, float low = MinThreshold, float high = MaxThreshold);
/// The level at that place, the inverse of levelFraction().
[[nodiscard]] float levelAt(float fraction, float low = MinThreshold, float high = MaxThreshold);

} // namespace rtype::voice
