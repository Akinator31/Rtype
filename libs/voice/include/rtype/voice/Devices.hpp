#pragma once

#include "rtype/voice/VoiceChannel.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

/// The devices of the voice (docs/architecture/voice.md#the-devices): the microphone and the voices played,
/// as interfaces. The client gives the real ones (Microphone, audio::VoiceStreams); the tests give fakes
/// (tests/helpers/FakeVoiceDevices.hpp), the CI having no sound card.
namespace rtype::voice
{

/// Where the voice of the player comes from: 48 kHz mono, signed 16 bits (Format.hpp).
///
/// Every call is made on the thread of the game loop. The samples come on another thread, the one of the
/// device, and wait for read() without a lock.
class Capture
{
public:
    virtual ~Capture() = default;

    /// Starts capturing. Nothing if it already does.
    /// @return false if there is no microphone, or it cannot be opened: the player listens only.
    virtual bool open() = 0;
    /// Stops capturing and forgets the samples not read. Nothing if closed.
    virtual void close() = 0;
    /// Takes the samples captured since the last call, oldest first, as many as fit.
    /// @return The samples taken.
    virtual std::size_t read(std::span<std::int16_t> samples) = 0;
};

/// Where the voices of the others are played: one stream per player, mixed with the sounds of the game.
/// The samples are 48 kHz mono, signed 16 bits, whatever carries the voice: a voice of Discord is played
/// here as one of the game.
///
/// Every call is made on the thread of the game loop. The samples are played on another thread, the one of
/// the device, which takes them without a lock and plays silence when there is none.
class Playback
{
public:
    virtual ~Playback() = default;

    /// Whether the voices can be heard: false without an audio output. Without it, the player has no voice.
    [[nodiscard]] virtual bool available() const = 0;
    /// Starts the stream of a player, silent until it is written to. Nothing if it is open.
    virtual void open(Slot slot) = 0;
    /// Stops it and forgets what was not played. Nothing if it is closed.
    virtual void close(Slot slot) = 0;
    /// Plays these samples after those written before.
    /// @return false if they do not fit: the stream is too far behind, they are dropped.
    virtual bool write(Slot slot, std::span<const std::int16_t> samples) = 0;
    /// The volume of a player, from 0 to MaxVolume.
    virtual void setVolume(Slot slot, float volume) = 0;
};

/// What the speakers play, all mixed: the voices, the music and the effects of the game, 48 kHz mono, signed
/// 16 bits. The reference of the echo cancellation (Processing.hpp): what the microphone hears of it is echo.
///
/// Every call is made on the thread of the game loop. The samples are mixed on another thread, the one of the
/// output, and wait for read() without a lock.
class Loopback
{
public:
    virtual ~Loopback() = default;

    /// Starts keeping what is played. Nothing if it already does.
    /// @return false without an audio output: nothing is played, there is no echo.
    virtual bool open() = 0;
    /// Stops keeping it and forgets the samples not read. Nothing if closed.
    virtual void close() = 0;
    /// Takes the samples played since the last call, oldest first, as many as fit.
    /// @return The samples taken.
    virtual std::size_t read(std::span<std::int16_t> samples) = 0;
};

} // namespace rtype::voice
