#include "rtype/audio/OutputLoopback.hpp"

#include "rtype/audio/AudioDevice.hpp"
#include "rtype/voice/Format.hpp"
#include "rtype/voice/RingBuffer.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace rtype::audio
{

namespace
{

/// The channels raylib mixes in, as floats (AUDIO_DEVICE_CHANNELS, AUDIO_DEVICE_FORMAT of raudio.c).
constexpr std::size_t OutputChannels = 2;
/// Frames mixed down at once, on the stack of the audio thread.
constexpr std::size_t Chunk = 256;

/// The samples of the OutputLoopback open, for the callback
voice::RingBuffer<std::int16_t> *current = nullptr;
bool exists = false;

/// The callback of the mix, on the audio thread of miniaudio: the two channels down to one, as signed 16
/// bits. What does not fit is dropped
void keep(void *buffer, unsigned int frames)
{
    const auto *mixed = static_cast<const float *>(buffer);
    std::array<std::int16_t, Chunk> mono{};
    for (std::size_t from = 0; from < frames; from += Chunk) {
        const std::size_t count = std::min<std::size_t>(Chunk, frames - from);
        for (std::size_t frame = 0; frame < count; ++frame) {
            const float *channels = mixed + ((from + frame) * OutputChannels);
            const float sample = std::clamp((channels[0] + channels[1]) * 0.5f, -1.0f, 1.0f);
            mono[frame] = static_cast<std::int16_t>(std::lround(sample * 32767.0f));
        }
        current->write(std::span<const std::int16_t>(mono.data(), count));
    }
}

} // namespace

struct OutputLoopback::State {
    bool available = false;
    bool open = false;
    voice::RingBuffer<std::int16_t> samples{static_cast<std::size_t>(voice::SampleRate)};
};

OutputLoopback::OutputLoopback(const AudioDevice &device) : _state(std::make_unique<State>())
{
    if (exists) {
        throw std::logic_error("An output loopback already exists");
    }
    exists = true;
    _state->available = device.isAvailable();
}

OutputLoopback::~OutputLoopback()
{
    close();
    exists = false;
}

bool OutputLoopback::open()
{
    if (_state->open) {
        return true;
    }
    if (!_state->available) {
        return false;
    }
    // Not mixed into yet: nothing writes the samples
    _state->samples.discard();
    current = &_state->samples;
    AttachAudioMixedProcessor(keep);
    _state->open = true;
    return true;
}

void OutputLoopback::close()
{
    if (!_state->open) {
        return;
    }
    // Under the lock of the audio thread: no callback runs after it
    DetachAudioMixedProcessor(keep);
    current = nullptr;
    _state->open = false;
}

std::size_t OutputLoopback::read(std::span<std::int16_t> samples)
{
    return _state->open ? _state->samples.read(samples) : 0;
}

} // namespace rtype::audio
