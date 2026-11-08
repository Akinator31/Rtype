#include "rtype/voice/Codec.hpp"

#include "rtype/log/Logger.hpp"

#include <opus.h>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace rtype::voice
{

namespace
{

const log::Logger &logger()
{
    static const log::Logger instance("voice");
    return instance;
}

constexpr int Frame = static_cast<int>(FrameSamples);

/// A call of Opus that cannot fail with the settings of this file, unless memory is out
void check(int result, const char *what)
{
    if (result != OPUS_OK) {
        logger().error("Opus could not {}: {}", what, opus_strerror(result));
        throw std::runtime_error(std::string("Opus could not ") + what);
    }
}

} // namespace

bool isValidFrame(std::span<const std::uint8_t> frame)
{
    if (frame.empty() || frame.size() > MaxFrameBytes) {
        return false;
    }
    return opus_packet_get_nb_samples(frame.data(), static_cast<opus_int32>(frame.size()), SampleRate) ==
           Frame;
}

struct Encoder::State {
    OpusEncoder *encoder = nullptr;

    ~State() { opus_encoder_destroy(encoder); }
};

Encoder::Encoder(const EncoderSettings &settings) : _state(std::make_unique<State>())
{
    int error = OPUS_OK;
    _state->encoder = opus_encoder_create(SampleRate, 1, OPUS_APPLICATION_VOIP, &error);
    check(error, "create an encoder");
    OpusEncoder *encoder = _state->encoder;
    check(opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE)), "set the signal");
    check(opus_encoder_ctl(encoder, OPUS_SET_VBR(1)), "set the variable bit rate");
    check(opus_encoder_ctl(encoder, OPUS_SET_BITRATE(settings.bitrate)), "set the bit rate");
    const int loss = std::clamp(settings.expectedLoss, 0, 100);
    check(opus_encoder_ctl(encoder, OPUS_SET_INBAND_FEC(loss > 0 ? 1 : 0)), "set the FEC");
    check(opus_encoder_ctl(encoder, OPUS_SET_PACKET_LOSS_PERC(loss)), "set the expected loss");
    check(opus_encoder_ctl(encoder, OPUS_SET_DTX(settings.discontinuous ? 1 : 0)), "set the DTX");
}

Encoder::~Encoder() = default;
Encoder::Encoder(Encoder &&) noexcept = default;
Encoder &Encoder::operator=(Encoder &&) noexcept = default;

EncodedFrame Encoder::encode(ConstPcm samples)
{
    EncodedFrame frame;
    const opus_int32 size = opus_encode(_state->encoder, samples.data(), Frame, frame.bytes.data(),
                                        static_cast<opus_int32>(MaxFrameBytes));
    if (size < 0) {
        // Only with wrong arguments, which this file never gives: nothing to send
        logger().error("Opus could not encode a frame: {}", opus_strerror(size));
        return frame;
    }
    frame.size = static_cast<std::size_t>(size);
    return frame;
}

void Encoder::reset()
{
    opus_encoder_ctl(_state->encoder, OPUS_RESET_STATE);
}

struct Decoder::State {
    OpusDecoder *decoder = nullptr;
    /// Decoded here first: a frame refused halfway leaves the samples of the caller as they were
    std::array<opus_int16, FrameSamples> buffer{};

    ~State() { opus_decoder_destroy(decoder); }

    bool run(const std::uint8_t *data, std::size_t size, bool fec, Pcm samples)
    {
        const int decoded =
            opus_decode(decoder, data, static_cast<opus_int32>(size), buffer.data(), Frame, fec ? 1 : 0);
        if (decoded != Frame) {
            return false;
        }
        std::ranges::copy(buffer, samples.begin());
        return true;
    }
};

Decoder::Decoder() : _state(std::make_unique<State>())
{
    int error = OPUS_OK;
    _state->decoder = opus_decoder_create(SampleRate, 1, &error);
    check(error, "create a decoder");
}

Decoder::~Decoder() = default;
Decoder::Decoder(Decoder &&) noexcept = default;
Decoder &Decoder::operator=(Decoder &&) noexcept = default;

bool Decoder::decode(std::span<const std::uint8_t> frame, Pcm samples)
{
    return isValidFrame(frame) && _state->run(frame.data(), frame.size(), false, samples);
}

bool Decoder::decodeFec(std::span<const std::uint8_t> next, Pcm samples)
{
    return isValidFrame(next) && _state->run(next.data(), next.size(), true, samples);
}

void Decoder::conceal(Pcm samples)
{
    if (!_state->run(nullptr, 0, false, samples)) {
        std::ranges::fill(samples, std::int16_t{0});
    }
}

void Decoder::reset()
{
    opus_decoder_ctl(_state->decoder, OPUS_RESET_STATE);
}

} // namespace rtype::voice
