#pragma once

#include "rtype/voice/Devices.hpp"

#include <cstdint>
#include <memory>
#include <span>

namespace rtype::audio
{

class AudioDevice;

/// The voices of the other players, played through the audio output (docs/architecture/voice.md): one
/// AudioStream of raylib per player, 48 kHz mono, mixed with the music and the effects, with its own volume.
///
/// Each stream has a RingBuffer of 200 ms that write() fills on the game loop and the callback of the stream
/// empties on the audio thread of miniaudio, which plays silence when it is empty: neither waits on the
/// other. The samples are PCM, whatever carried them: the voices of Discord are played here too.
///
/// @code
/// audio::VoiceStreams voices(audioDevice);
/// voice::GameVoice voice(session, microphone, voices);
/// @endcode
///
/// Without an audio output (AudioDevice::isAvailable()), it is not available and plays nothing. Only one can
/// exist at a time: the callbacks of raylib take no pointer, they find it in a variable of this file. It must
/// be destroyed before the AudioDevice.
class VoiceStreams final : public voice::Playback
{
    struct State;
    std::unique_ptr<State> _state;

public:
    /// @throws std::logic_error if another one exists.
    explicit VoiceStreams(const AudioDevice &device);
    /// Closes every stream.
    ~VoiceStreams() override;

    VoiceStreams(const VoiceStreams &) = delete;
    VoiceStreams &operator=(const VoiceStreams &) = delete;

    [[nodiscard]] bool available() const override;
    void open(voice::Slot slot) override;
    void close(voice::Slot slot) override;
    bool write(voice::Slot slot, std::span<const std::int16_t> samples) override;
    void setVolume(voice::Slot slot, float volume) override;
};

} // namespace rtype::audio
