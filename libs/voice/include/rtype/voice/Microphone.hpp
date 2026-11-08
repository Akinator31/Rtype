#pragma once

#include "rtype/voice/Devices.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace rtype::voice
{

/// A microphone of the system, the default one or the one chosen in the settings, captured by the miniaudio
/// that raylib compiles: 48 kHz mono, signed 16 bits, converted by miniaudio from what the device gives.
///
/// Its callback, on the audio thread of miniaudio, writes the samples to a RingBuffer of one second, which
/// read() empties on the game loop: neither waits on the other. A game loop that does not read for a second
/// loses the oldest samples.
///
/// A microphone unplugged while open is followed: the next read() opens the default one of the system
/// instead, or the one chosen if it is back.
///
/// @code
/// voice::Microphone microphone;
/// microphone.setDevice(preferences.inputDevice);    // "" for the default of the system
/// if (!microphone.open()) {
///     // No microphone: the player listens only
/// }
/// @endcode
///
/// miniaudio stays out of this header.
class Microphone final : public Capture
{
    struct State;
    std::unique_ptr<State> _state;

public:
    Microphone();
    /// Closes the device if open.
    ~Microphone() override;

    Microphone(const Microphone &) = delete;
    Microphone &operator=(const Microphone &) = delete;

    bool open() override;
    void close() override;
    std::size_t read(std::span<std::int16_t> samples) override;

    /// The device is open.
    [[nodiscard]] bool isOpen() const;

    /// The names of the microphones of the system, as setDevice() takes them. Empty without a backend of
    /// audio, or without a microphone.
    [[nodiscard]] std::vector<std::string> devices();
    /// Captures from the microphone of this name from now on: at once if open, at the next open() otherwise.
    /// Empty, or a name that is not one of devices(), for the default of the system.
    void setDevice(const std::string &name);
    /// The microphone chosen by setDevice(), empty for the default of the system.
    [[nodiscard]] const std::string &device() const;
};

} // namespace rtype::voice
