#include "rtype/voice/Microphone.hpp"

#include "rtype/log/Logger.hpp"
#include "rtype/voice/Format.hpp"
#include "rtype/voice/RingBuffer.hpp"

// The miniaudio of raylib, without its implementation, which raylib compiles: one copy of it. The defines are
// those raylib compiles it with (raudio.c), so that its structures have the same layout here
#define MA_NO_JACK
#define MA_NO_WAV
#define MA_NO_FLAC
#define MA_NO_MP3
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MA_NO_GENERATION
#include "external/miniaudio.h"

namespace rtype::voice
{

namespace
{

const log::Logger &logger()
{
    static const log::Logger instance("voice");
    return instance;
}

/// The period of the device: 10 ms.
constexpr ma_uint32 PeriodMilliseconds = 10;

} // namespace

struct Microphone::State {
    ma_device device{};
    bool open = false;
    /// One second of samples
    RingBuffer<std::int16_t> samples{static_cast<std::size_t>(SampleRate)};

    /// On the audio thread of miniaudio
    static void onData(ma_device *device, void * /*output*/, const void *input, ma_uint32 frames)
    {
        auto *state = static_cast<State *>(device->pUserData);
        if (input != nullptr) {
            (void)state->samples.write({static_cast<const std::int16_t *>(input), frames});
        }
    }
};

Microphone::Microphone() : _state(std::make_unique<State>()) {}

Microphone::~Microphone()
{
    close();
}

bool Microphone::open()
{
    if (_state->open) {
        return true;
    }
    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_s16;
    config.capture.channels = 1;
    config.sampleRate = SampleRate;
    config.periodSizeInMilliseconds = PeriodMilliseconds;
    config.dataCallback = &State::onData;
    config.pUserData = _state.get();
    if (const ma_result result = ma_device_init(nullptr, &config, &_state->device); result != MA_SUCCESS) {
        logger().warning("Cannot open the microphone: {}", ma_result_description(result));
        return false;
    }
    if (const ma_result result = ma_device_start(&_state->device); result != MA_SUCCESS) {
        logger().warning("Cannot start the microphone: {}", ma_result_description(result));
        ma_device_uninit(&_state->device);
        return false;
    }
    _state->open = true;
    logger().info("Microphone open: {}", _state->device.capture.name);
    return true;
}

void Microphone::close()
{
    if (!_state->open) {
        return;
    }
    // Waits for the callback running, if any: none after
    ma_device_uninit(&_state->device);
    _state->open = false;
    _state->samples.discard();
    logger().info("Microphone closed");
}

std::size_t Microphone::read(std::span<std::int16_t> samples)
{
    return _state->open ? _state->samples.read(samples) : 0;
}

bool Microphone::isOpen() const
{
    return _state->open;
}

} // namespace rtype::voice
