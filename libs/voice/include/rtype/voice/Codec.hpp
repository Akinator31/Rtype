#pragma once

#include "rtype/voice/Format.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

/// The encoder and the decoder of the voice (docs/architecture/voice.md#the-codec, ADR 0035): Opus, 48 kHz
/// mono, frames of 20 ms. Opus stays out of this header.
namespace rtype::voice
{

/// One frame of samples, signed 16 bits.
using Pcm = std::span<std::int16_t, FrameSamples>;
using ConstPcm = std::span<const std::int16_t, FrameSamples>;

/// How the encoder encodes.
struct EncoderSettings {
    /// The target bit rate, variable: about 60 bytes per frame at 24 kbit/s.
    int bitrate = 24000;
    /// The share of frames expected to be lost, in percent: the more, the more of each frame is spent on
    /// rebuilding the one before it (in-band FEC). 0 turns the FEC off.
    int expectedLoss = 10;
    /// Silence encoded in 1 or 2 bytes, which need not be sent, nor the updates of its noise (DTX).
    bool discontinuous = true;
};

/// One frame encoded.
struct EncodedFrame {
    std::array<std::uint8_t, MaxFrameBytes> bytes{};
    std::size_t size = 0;
    /// Encoded while the encoder is in DTX: a silence, or the update of its noise Opus gives every 400 ms.
    bool discontinued = false;

    [[nodiscard]] std::span<const std::uint8_t> data() const { return {bytes.data(), size}; }
    /// A frame of silence, with nothing to send: the decoder of the others conceals it, fading to silence.
    [[nodiscard]] bool silence() const { return size <= 2 || discontinued; }
};

/// Encodes the voice of the player, one frame of 20 ms at a time.
///
/// @code
/// rtype::voice::Encoder encoder;
/// const rtype::voice::EncodedFrame frame = encoder.encode(samples);   // 960 samples
/// if (!frame.silence()) {
///     session.sendVoice(frame.data());
/// }
/// @endcode
class Encoder
{
    struct State;
    std::unique_ptr<State> _state;

public:
    explicit Encoder(const EncoderSettings &settings = {});
    ~Encoder();

    Encoder(Encoder &&) noexcept;
    Encoder &operator=(Encoder &&) noexcept;
    Encoder(const Encoder &) = delete;
    Encoder &operator=(const Encoder &) = delete;

    /// Encodes the next frame. Never more than MaxFrameBytes: the encoder lowers the quality of a frame that
    /// would not fit.
    [[nodiscard]] EncodedFrame encode(ConstPcm samples);
    /// Forgets the frames before: the next one starts a new talk spurt.
    void reset();
};

/// Decodes the voice of one player, one frame of 20 ms at a time, in the order of the frames. Each player
/// needs its own: the decoder keeps what it heard to rebuild what it did not.
///
/// @code
/// if (received) {
///     decoder.decode(frame, samples);
/// } else if (next) {
///     decoder.decodeFec(next, samples);    // the lost frame, from the one after it; then decode(next) as
///     usual
/// } else {
///     decoder.conceal(samples);
/// }
/// @endcode
///
/// A frame comes from the network: anything may be in it. A frame that is not a frame of 20 ms of Opus, of 1
/// to MaxFrameBytes bytes, is refused, and the samples are left as they were.
class Decoder
{
    struct State;
    std::unique_ptr<State> _state;

public:
    Decoder();
    ~Decoder();

    Decoder(Decoder &&) noexcept;
    Decoder &operator=(Decoder &&) noexcept;
    Decoder(const Decoder &) = delete;
    Decoder &operator=(const Decoder &) = delete;

    /// Decodes a frame received. False if it is refused.
    [[nodiscard]] bool decode(std::span<const std::uint8_t> frame, Pcm samples);
    /// Rebuilds the frame lost just before `next` from the copy `next` carries of it (in-band FEC), or
    /// conceals it if `next` carries none. False if `next` is refused: then conceal().
    [[nodiscard]] bool decodeFec(std::span<const std::uint8_t> next, Pcm samples);
    /// Makes up a frame lost with nothing to rebuild it from, from the ones before; fades to silence when
    /// called again and again.
    void conceal(Pcm samples);
    /// Forgets the frames before: the next one starts a new talk spurt.
    void reset();
};

/// Whether a frame is one the decoder accepts: 1 to MaxFrameBytes bytes, a valid Opus packet of 20 ms.
[[nodiscard]] bool isValidFrame(std::span<const std::uint8_t> frame);

} // namespace rtype::voice
