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
#include <memory>
#include <optional>
#include <thread>

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

/// The context of miniaudio: shared by the microphone and the devices still closing, uninitialised after the
/// last of them.
struct Context {
    ma_context context{};
    bool ready = false;

    Context() = default;
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
    ~Context()
    {
        if (ready) {
            ma_context_uninit(&context);
        }
    }
};

/// One opening of a microphone: its samples and its flags its own, so that a device closing apart touches
/// nothing of the next one.
struct CaptureDevice {
    std::shared_ptr<Context> context;
    ma_device device{};
    /// The device stopped by itself, unplugged: set on the thread of miniaudio, read on the game loop.
    std::atomic<bool> lost{false};
    /// The game loop closed the device: its stop is not a loss, and its samples go nowhere.
    std::atomic<bool> closing{false};
    /// One second of samples
    RingBuffer<std::int16_t> samples{static_cast<std::size_t>(SampleRate)};

    /// On the audio thread of miniaudio
    static void onData(ma_device *device, void * /*output*/, const void *input, ma_uint32 frames)
    {
        auto *self = static_cast<CaptureDevice *>(device->pUserData);
        if (input != nullptr && !self->closing.load(std::memory_order_relaxed)) {
            (void)self->samples.write({static_cast<const std::int16_t *>(input), frames});
        }
    }

    /// On a thread of miniaudio
    static void onNotification(const ma_device_notification *notification)
    {
        auto *self = static_cast<CaptureDevice *>(notification->pDevice->pUserData);
        if (notification->type == ma_device_notification_type_stopped && !self->closing.load()) {
            self->lost.store(true);
        }
    }
};

/// Uninitialises a device on a thread of its own, which owns it until then: the game loop never waits for
/// it. With a Bluetooth headset on PipeWire, whose profile another program (Discord) just changed by opening
/// its microphone, ma_device_uninit() can wait for a long time, or for ever.
void closeApart(std::unique_ptr<CaptureDevice> device)
{
    device->closing.store(true);
    std::thread([device = std::move(device)] { ma_device_uninit(&device->device); }).detach();
}

} // namespace

struct Microphone::State {
    std::shared_ptr<Context> context;
    /// The device open, none while closed.
    std::unique_ptr<CaptureDevice> device;
    /// The name of the microphone chosen, empty for the default.
    std::string wanted;

    /// The context of the devices, made at the first need: it lists the microphones, and opens them.
    bool readyContext()
    {
        if (!context) {
            auto made = std::make_shared<Context>();
            if (const ma_result result = ma_context_init(nullptr, 0, nullptr, &made->context);
                result != MA_SUCCESS) {
                logger().warning("No audio backend for the microphone: {}", ma_result_description(result));
                return false;
            }
            made->ready = true;
            context = std::move(made);
        }
        return true;
    }

    /// The id of the microphone of this name, if it is plugged.
    std::optional<ma_device_id> find(const std::string &name)
    {
        ma_device_info *inputs = nullptr;
        ma_uint32 count = 0;
        if (name.empty() ||
            ma_context_get_devices(&context->context, nullptr, nullptr, &inputs, &count) != MA_SUCCESS) {
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
        if (device) {
            closeApart(std::move(device));
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
    if (_state->device) {
        return true;
    }
    if (!_state->readyContext()) {
        return false;
    }
    const std::optional<ma_device_id> chosen = _state->find(_state->wanted);
    if (!_state->wanted.empty() && !chosen) {
        logger().warning("Microphone \"{}\" not found: the default one is used", _state->wanted);
    }
    auto opened = std::make_unique<CaptureDevice>();
    opened->context = _state->context;
    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.pDeviceID = chosen ? &*chosen : nullptr;
    config.capture.format = ma_format_s16;
    config.capture.channels = 1;
    config.sampleRate = SampleRate;
    config.periodSizeInMilliseconds = PeriodMilliseconds;
    config.dataCallback = &CaptureDevice::onData;
    config.notificationCallback = &CaptureDevice::onNotification;
    config.pUserData = opened.get();
    if (const ma_result result = ma_device_init(&_state->context->context, &config, &opened->device);
        result != MA_SUCCESS) {
        logger().warning("Cannot open the microphone: {}", ma_result_description(result));
        return false;
    }
    if (const ma_result result = ma_device_start(&opened->device); result != MA_SUCCESS) {
        logger().warning("Cannot start the microphone: {}", ma_result_description(result));
        closeApart(std::move(opened));
        return false;
    }
    logger().info("Microphone open: {}", opened->device.capture.name);
    _state->device = std::move(opened);
    return true;
}

void Microphone::close()
{
    if (!_state->device) {
        return;
    }
    _state->stop();
    logger().info("Microphone closed");
}

std::size_t Microphone::read(std::span<std::int16_t> samples)
{
    if (_state->device && _state->device->lost.load()) {
        logger().warning("Microphone lost: it is opened again");
        _state->stop();
        if (!open()) {
            return 0;
        }
    }
    return _state->device ? _state->device->samples.read(samples) : 0;
}

bool Microphone::isOpen() const
{
    return _state->device != nullptr;
}

std::vector<std::string> Microphone::devices()
{
    std::vector<std::string> names;
    ma_device_info *inputs = nullptr;
    ma_uint32 count = 0;
    if (!_state->readyContext() ||
        ma_context_get_devices(&_state->context->context, nullptr, nullptr, &inputs, &count) != MA_SUCCESS) {
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
    if (_state->device) {
        _state->stop();
        (void)open();
    }
}

const std::string &Microphone::device() const
{
    return _state->wanted;
}

} // namespace rtype::voice
