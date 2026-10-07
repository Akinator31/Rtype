#pragma once

namespace rtype::audio
{

/// Settings applied when the audio device is opened.
struct AudioSettings {
    /// Whether to open the audio output. When false, the game runs silently: useful for tests, servers, or a
    /// "no sound" command-line option.
    bool enabled = true;
    /// Volume of everything that is played, between 0 and 1.
    float masterVolume = 1.0f;
};

/// The audio output, open for as long as this object exists.
///
/// The output is opened by the constructor and closed by the destructor (RAII). If it cannot be opened, for
/// instance on a computer without sound card, the game keeps running silently: a warning is logged and every
/// sound is ignored. Only one device can exist at a time.
///
/// Create it after the Window, so that raylib's messages about the audio output go to the log too.
class AudioDevice
{
private:
    bool _available = false;
    float _masterVolume = 1.0f;

public:
    /// Opens the default audio output.
    /// @throws std::logic_error if a device already exists.
    explicit AudioDevice(const AudioSettings &settings = {});
    ~AudioDevice();

    AudioDevice(const AudioDevice &) = delete;
    AudioDevice &operator=(const AudioDevice &) = delete;
    AudioDevice(AudioDevice &&) = delete;
    AudioDevice &operator=(AudioDevice &&) = delete;

    /// Tells whether sounds can be heard: false if audio is disabled or the output could not be opened.
    [[nodiscard]] bool isAvailable() const { return _available; }

    /// Sets the volume of everything that is played, between 0 (silent) and 1. Clamped to this range.
    void setMasterVolume(float volume);
    /// Volume of everything that is played, between 0 and 1.
    [[nodiscard]] float masterVolume() const { return _masterVolume; }
};

} // namespace rtype::audio
