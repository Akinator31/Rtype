#include "rtype/audio/AudioDevice.hpp"

#include "rtype/log/Logger.hpp"

#include <raylib.h>

#include <algorithm>
#include <stdexcept>

namespace rtype::audio
{

namespace
{

bool deviceExists = false;

} // namespace

AudioDevice::AudioDevice(const AudioSettings &settings)
{
    if (deviceExists) {
        throw std::logic_error("An audio device already exists");
    }
    deviceExists = true;

    const log::Logger logger("audio");
    if (!settings.enabled) {
        logger.info("Audio is disabled: the game runs silently");
        return;
    }
    InitAudioDevice();
    _available = IsAudioDeviceReady();
    if (!_available) {
        // Not an error: the game is still playable without sound
        logger.warning("Cannot open the audio output: the game runs silently");
        return;
    }
    setMasterVolume(settings.masterVolume);
}

AudioDevice::~AudioDevice()
{
    if (_available) {
        CloseAudioDevice();
    }
    deviceExists = false;
}

void AudioDevice::setMasterVolume(float volume)
{
    _masterVolume = std::clamp(volume, 0.0f, 1.0f);
    if (_available) {
        SetMasterVolume(_masterVolume);
    }
}

} // namespace rtype::audio
