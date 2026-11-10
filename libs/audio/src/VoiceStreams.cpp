#include "rtype/audio/VoiceStreams.hpp"

#include "rtype/audio/AudioDevice.hpp"
#include "rtype/voice/Format.hpp"
#include "rtype/voice/RingBuffer.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <optional>
#include <stdexcept>
#include <utility>

namespace rtype::audio
{

namespace
{

/// 200 ms of voice waiting to be played, per player: more is a stream far behind, whose samples are dropped.
constexpr std::size_t StreamSamples = static_cast<std::size_t>(voice::SampleRate) / 5;
constexpr unsigned int SampleBits = 16;

struct Stream {
    AudioStream stream{};
    bool open = false;
    voice::RingBuffer<std::int16_t> samples{StreamSamples};
};

/// The streams of the VoiceStreams that exists, for the callbacks
std::array<Stream, voice::MaxParticipants> *current = nullptr;

/// The callback of the stream of a slot, on the audio thread of miniaudio: what is there, then silence
template <std::size_t Slot>
void fill(void *buffer, unsigned int frames)
{
    std::span<std::int16_t> out(static_cast<std::int16_t *>(buffer), frames);
    const std::size_t read = (*current)[Slot].samples.read(out);
    std::fill(out.begin() + static_cast<std::ptrdiff_t>(read), out.end(), std::int16_t{0});
}

template <std::size_t... Slots>
constexpr std::array<AudioCallback, sizeof...(Slots)> callbacks(std::index_sequence<Slots...> /*slots*/)
{
    return {&fill<Slots>...};
}

constexpr std::array<AudioCallback, voice::MaxParticipants> Callbacks =
    callbacks(std::make_index_sequence<voice::MaxParticipants>{});

} // namespace

struct VoiceStreams::State {
    bool available = false;
    std::array<Stream, voice::MaxParticipants> streams;
};

VoiceStreams::VoiceStreams(const AudioDevice &device) : _state(std::make_unique<State>())
{
    if (current != nullptr) {
        throw std::logic_error("Voice streams already exist");
    }
    current = &_state->streams;
    _state->available = device.isAvailable();
}

VoiceStreams::~VoiceStreams()
{
    for (voice::Slot slot = 0; slot < voice::MaxParticipants; ++slot) {
        close(slot);
    }
    current = nullptr;
}

bool VoiceStreams::available() const
{
    return _state->available;
}

void VoiceStreams::open(voice::Slot slot)
{
    if (!_state->available || slot >= voice::MaxParticipants || _state->streams[slot].open) {
        return;
    }
    Stream &stream = _state->streams[slot];
    // Not played yet: nothing reads the samples
    stream.samples.discard();
    stream.stream = LoadAudioStream(voice::SampleRate, SampleBits, 1);
    if (!IsAudioStreamValid(stream.stream)) {
        return;
    }
    SetAudioStreamCallback(stream.stream, Callbacks[slot]);
    PlayAudioStream(stream.stream);
    stream.open = true;
}

void VoiceStreams::close(voice::Slot slot)
{
    if (slot >= voice::MaxParticipants || !_state->streams[slot].open) {
        return;
    }
    Stream &stream = _state->streams[slot];
    // Under the lock of the audio thread: no callback runs after it
    UnloadAudioStream(stream.stream);
    stream.open = false;
}

bool VoiceStreams::write(voice::Slot slot, std::span<const std::int16_t> samples)
{
    if (slot >= voice::MaxParticipants || !_state->streams[slot].open) {
        return false;
    }
    return _state->streams[slot].samples.write(samples) == samples.size();
}

void VoiceStreams::setVolume(voice::Slot slot, float volume)
{
    if (slot < voice::MaxParticipants && _state->streams[slot].open) {
        SetAudioStreamVolume(_state->streams[slot].stream, std::clamp(volume, 0.0f, voice::MaxVolume));
    }
}

} // namespace rtype::audio
