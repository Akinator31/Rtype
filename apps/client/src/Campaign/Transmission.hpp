#pragma once

#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/ecs/Entity/Entity.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace rtype::client
{

/// The steps of a Transmission, one after the other.
enum class TransmissionPhase {
    /// A black screen, the hum of the channel alone.
    Silence,
    /// The radio crackles open, and the signal blinks with a beep, then the channel connects.
    Signal,
    /// The lines are written one after the other, letter by letter, like a teletype.
    Typing,
    /// The last line is written: the end of the transmission shows, with its over and out.
    Closing,
    /// Everything is shown: the transmission waits for the player to go on.
    Waiting,
    /// The player went on: the onEnd of the transmission has been called.
    Done,
};

/// Number of TransmissionPhases: the size of the tables indexed by them.
inline constexpr std::size_t TransmissionPhaseCount = static_cast<std::size_t>(TransmissionPhase::Done) + 1;

/// The sounds of a transmission.
struct TransmissionSounds {
    audio::SoundId statics;
    audio::SoundId beep;
    audio::SoundId connect;
    audio::SoundId type;
    audio::SoundId line;
    audio::SoundId end;
};

/// A line of a transmission: a Text written up to the letters typed so far.
struct TransmissionLine {
    ecs::entity_t entity = 0;
    /// The whole line, in UTF-8.
    std::string text;
    /// How fast it is written, in characters per second. An empty line is a short pause.
    float speed = 45.0f;
};

/// A message received on a black screen, like a radio transmission: after a moment of silence, the radio
/// crackles, "INCOMING TRANSMISSION" blinks, then the lines are written letter by letter, each key with its
/// sound, and the end of the transmission shows. The TransmissionSystem plays it; the player can skip to its
/// end, then goes on, which calls onEnd.
struct Transmission {
    std::vector<TransmissionLine> lines;
    /// "INCOMING TRANSMISSION" and the name of the channel, which blink then stay.
    ecs::entity_t signal = 0;
    ecs::entity_t channel = 0;
    /// "END OF TRANSMISSION", hidden until then.
    ecs::entity_t over = 0;
    /// What the player can press: skip, then continue.
    ecs::entity_t hint = 0;
    std::string skipHint;
    std::string continueHint;
    TransmissionSounds sounds;
    std::function<void()> onEnd{};

    TransmissionPhase phase = TransmissionPhase::Silence;
    /// Time since the start of the phase, and since the start of the transmission, in seconds.
    float time = 0.0f;
    float clock = 0.0f;
    /// The line being written, and how many of its characters are written.
    std::size_t line = 0;
    float typed = 0.0f;
    /// Time left before the next line is written.
    float pause = 0.0f;
    /// Beeps played while the signal blinks.
    int beeps = 0;
    /// Keys typed so far, which vary the pitch of their sound.
    std::uint32_t keys = 0;
};

/// Number of characters of a text in UTF-8.
[[nodiscard]] std::size_t countCharacters(std::string_view text);

/// The first `count` characters of a text in UTF-8: never cut inside a character.
[[nodiscard]] std::string_view firstCharacters(std::string_view text, std::size_t count);

/// Cuts a text into lines no wider than `width`, between its words. A word wider than `width` gets a line of
/// its own. An empty text gives one empty line.
/// @param measure Width of a text once drawn.
[[nodiscard]] std::vector<std::string> wrapWords(const std::string &text, float width,
                                                 const std::function<float(const std::string &)> &measure);

} // namespace rtype::client
