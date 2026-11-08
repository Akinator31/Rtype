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

#include <atomic>
#include <optional>

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
    ma_context context{};
    bool contextReady = false;
    ma_device device{};
    bool open = false;
    /// The name of the microphone chosen, empty for the default.
    std::string wanted;
    /// The device stopped by itself, unplugged: set on the thread of miniaudio, read on the game loop.
    std::atomic<bool> lost{false};
    /// The game loop is stopping the device: its stop is not a loss.
    std::atomic<bool> closing{false};
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

    /// On a thread of miniaudio
    static void onNotification(const ma_device_notification *notification)
    {
        auto *state = static_cast<State *>(notification->pDevice->pUserData);
        if (notification->type == ma_device_notification_type_stopped && !state->closing.load()) {
            state->lost.store(true);
        }
    }

    /// The context of the devices, made at the first need: it lists the microphones, and opens them.
    bool readyContext()
    {
        if (!contextReady) {
            if (const ma_result result = ma_context_init(nullptr, 0, nullptr, &context);
                result != MA_SUCCESS) {
                logger().warning("No audio backend for the microphone: {}", ma_result_description(result));
                return false;
            }
            contextReady = true;
        }
        return true;
    }

    /// The id of the microphone of this name, if it is plugged.
    std::optional<ma_device_id> find(const std::string &name)
    {
        ma_device_info *inputs = nullptr;
        ma_uint32 count = 0;
        if (name.empty() ||
            ma_context_get_devices(&context, nullptr, nullptr, &inputs, &count) != MA_SUCCESS) {
            return std::nullopt;
        }
        for (ma_uint32 index = 0; index < count; ++index) {
            if (name == inputs[index].name) {
                return inputs[index].id;
            }
        }
        return std::nullopt;
    }

    void stop()
    {
        closing.store(true);
        // Waits for the callback running, if any: none after
        ma_device_uninit(&device);
        closing.store(false);
        lost.store(false);
        open = false;
        samples.discard();
    }
};

Microphone::Microphone() : _state(std::make_unique<State>()) {}

Microphone::~Microphone()
{
    close();
    if (_state->contextReady) {
        ma_context_uninit(&_state->context);
    }
}

bool Microphone::open()
{
    if (_state->open) {
        return true;
    }
    if (!_state->readyContext()) {
        return false;
    }
    const std::optional<ma_device_id> chosen = _state->find(_state->wanted);
    if (!_state->wanted.empty() && !chosen) {
        logger().warning("Microphone \"{}\" not found: the default one is used", _state->wanted);
    }
    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.pDeviceID = chosen ? &*chosen : nullptr;
    config.capture.format = ma_format_s16;
    config.capture.channels = 1;
    config.sampleRate = SampleRate;
    config.periodSizeInMilliseconds = PeriodMilliseconds;
    config.dataCallback = &State::onData;
    config.notificationCallback = &State::onNotification;
    config.pUserData = _state.get();
    if (const ma_result result = ma_device_init(&_state->context, &config, &_state->device);
        result != MA_SUCCESS) {
        logger().warning("Cannot open the microphone: {}", ma_result_description(result));
        return false;
    }
    if (const ma_result result = ma_device_start(&_state->device); result != MA_SUCCESS) {
        logger().warning("Cannot start the microphone: {}", ma_result_description(result));
        _state->closing.store(true);
        ma_device_uninit(&_state->device);
        _state->closing.store(false);
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
    _state->stop();
    logger().info("Microphone closed");
}

std::size_t Microphone::read(std::span<std::int16_t> samples)
{
    if (_state->open && _state->lost.load()) {
        logger().warning("Microphone lost: it is opened again");
        _state->stop();
        if (!open()) {
            return 0;
        }
    }
    return _state->open ? _state->samples.read(samples) : 0;
}

bool Microphone::isOpen() const
{
    return _state->open;
}

std::vector<std::string> Microphone::devices()
{
    std::vector<std::string> names;
    ma_device_info *inputs = nullptr;
    ma_uint32 count = 0;
    if (!_state->readyContext() ||
        ma_context_get_devices(&_state->context, nullptr, nullptr, &inputs, &count) != MA_SUCCESS) {
        return names;
    }
    for (ma_uint32 index = 0; index < count; ++index) {
        names.emplace_back(inputs[index].name);
    }
    return names;
}

void Microphone::setDevice(const std::string &name)
{
    if (name == _state->wanted) {
        return;
    }
    _state->wanted = name;
    if (_state->open) {
        _state->stop();
        (void)open();
    }
}

const std::string &Microphone::device() const
{
    return _state->wanted;
}

} // namespace rtype::voice
