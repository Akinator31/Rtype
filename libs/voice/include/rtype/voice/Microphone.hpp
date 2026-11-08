#pragma once

#include "rtype/voice/Devices.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace rtype::voice
{

/// The default microphone of the system, captured by the miniaudio that raylib compiles: 48 kHz mono, signed
/// 16 bits, converted by miniaudio from what the device gives.
///
/// Its callback, on the audio thread of miniaudio, writes the samples to a RingBuffer of one second, which
/// read() empties on the game loop: neither waits on the other. A game loop that does not read for a second
/// loses the oldest samples.
///
/// @code
/// voice::Microphone microphone;
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
};

} // namespace rtype::voice
