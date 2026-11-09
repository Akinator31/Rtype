#pragma once

#include "rtype/voice/Devices.hpp"

#include <cstdint>
#include <memory>
#include <span>

namespace rtype::audio
{

class AudioDevice;

/// What the audio output plays, all mixed: the music, the effects and the voices of the others, taken where
/// raylib has mixed them (AttachAudioMixedProcessor), before the master volume. The reference of the echo
/// cancellation of the microphone (docs/architecture/voice.md#the-sound-processing).
///
/// The output runs at 48 kHz, the rate of the voice (libs/CMakeLists.txt): the callback, on the audio thread
/// of miniaudio, mixes its two channels down to one and writes them to a RingBuffer of one second, which
/// read() empties on the game loop: neither waits on the other.
///
/// @code
/// audio::OutputLoopback loopback(audioDevice);
/// voice::GameVoice voice(session, microphone, voices, &loopback);
/// @endcode
///
/// Without an audio output, open() fails: nothing is played, there is no echo. Only one can exist at a time:
/// the callback of raylib takes no pointer, it finds it in a variable of this file. It must be destroyed
/// before the AudioDevice.
class OutputLoopback final : public voice::Loopback
{
    struct State;
    std::unique_ptr<State> _state;

public:
    /// @throws std::logic_error if another one exists.
    explicit OutputLoopback(const AudioDevice &device);
    /// Closes it if open.
    ~OutputLoopback() override;

    OutputLoopback(const OutputLoopback &) = delete;
    OutputLoopback &operator=(const OutputLoopback &) = delete;

    bool open() override;
    void close() override;
    std::size_t read(std::span<std::int16_t> samples) override;
};

} // namespace rtype::audio
