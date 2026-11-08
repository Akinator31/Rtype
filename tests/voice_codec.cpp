#include "rtype/voice/Codec.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>
#include <vector>

using rtype::voice::Decoder;
using rtype::voice::EncodedFrame;
using rtype::voice::Encoder;
using rtype::voice::FrameSamples;
using rtype::voice::MaxFrameBytes;
using rtype::voice::SampleRate;

namespace
{

using Frame = std::array<std::int16_t, FrameSamples>;

/// A voice-like sound: a pitch gliding from 140 to 220 Hz and back over a second, with its harmonics
std::vector<Frame> voice(std::size_t frames)
{
    std::vector<Frame> result(frames);
    double phase = 0.0;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (std::size_t i = 0; i < FrameSamples; ++i) {
            const double t = static_cast<double>(frame * FrameSamples + i) / SampleRate;
            const double pitch = 180.0 + 40.0 * std::sin(2.0 * std::numbers::pi * t);
            phase += 2.0 * std::numbers::pi * pitch / SampleRate;
            double value = 0.0;
            for (int harmonic = 1; harmonic <= 8; ++harmonic) {
                value += std::sin(harmonic * phase) / harmonic;
            }
            result[frame][i] = static_cast<std::int16_t>(value * 4000.0);
        }
    }
    return result;
}

double rms(const std::vector<Frame> &frames, std::size_t from)
{
    double sum = 0.0;
    std::size_t count = 0;
    for (std::size_t frame = from; frame < frames.size(); ++frame) {
        for (const std::int16_t sample : frames[frame]) {
            sum += static_cast<double>(sample) * sample;
            ++count;
        }
    }
    return std::sqrt(sum / static_cast<double>(count));
}

/// The energy of the difference between two frames
double distance(const Frame &a, const Frame &b)
{
    double sum = 0.0;
    for (std::size_t i = 0; i < FrameSamples; ++i) {
        const double difference = static_cast<double>(a[i]) - b[i];
        sum += difference * difference;
    }
    return sum;
}

std::vector<EncodedFrame> encodeAll(Encoder &encoder, const std::vector<Frame> &frames)
{
    std::vector<EncodedFrame> encoded;
    for (const Frame &frame : frames) {
        encoded.push_back(encoder.encode(frame));
    }
    return encoded;
}

} // namespace

TEST_CASE("A voice encoded then decoded keeps its level and its pitch", "[voice]")
{
    const std::vector<Frame> original = voice(50);
    Encoder encoder;
    Decoder decoder;
    std::vector<Frame> decoded(original.size());
    for (std::size_t frame = 0; frame < original.size(); ++frame) {
        const EncodedFrame encoded = encoder.encode(original[frame]);
        REQUIRE(decoder.decode(encoded.data(), decoded[frame]));
    }

    // The first frames, while the codec settles, are left out
    const double level = 20.0 * std::log10(rms(decoded, 10) / rms(original, 10));
    CHECK(std::abs(level) < 3.0);

    // The pitch of a frame: the strongest autocorrelation between the lags of 400 and 80 Hz. Frame 37 is at
    // 0.75 s, the lowest of the glide, 140 Hz
    const Frame &frame = decoded[37];
    std::size_t bestLag = 0;
    double best = 0.0;
    for (std::size_t lag = SampleRate / 400; lag <= SampleRate / 80; ++lag) {
        double sum = 0.0;
        for (std::size_t i = 0; i + lag < FrameSamples; ++i) {
            sum += static_cast<double>(frame[i]) * frame[i + lag];
        }
        if (sum > best) {
            best = sum;
            bestLag = lag;
        }
    }
    const double pitch = static_cast<double>(SampleRate) / static_cast<double>(bestLag);
    CHECK(pitch > 130.0);
    CHECK(pitch < 150.0);
}

TEST_CASE("A voice frame is about 60 bytes at 24 kbit/s", "[voice]")
{
    Encoder encoder;
    std::size_t total = 0;
    const std::vector<EncodedFrame> encoded = encodeAll(encoder, voice(50));
    for (const EncodedFrame &frame : encoded) {
        total += frame.size;
    }
    const std::size_t average = total / encoded.size();
    CHECK(average > 40);
    CHECK(average < 80);
}

TEST_CASE("The encoder never gives a frame bigger than the largest one", "[voice]")
{
    // Loud noise at the highest bit rate of Opus: it would take far more than the largest frame
    Encoder encoder({.bitrate = 510000});
    std::mt19937 random(42);
    std::uniform_int_distribution<int> sample(-32768, 32767);
    for (int i = 0; i < 50; ++i) {
        Frame frame;
        for (std::int16_t &value : frame) {
            value = static_cast<std::int16_t>(sample(random));
        }
        const EncodedFrame encoded = encoder.encode(frame);
        CHECK(encoded.size > 0);
        CHECK(encoded.size <= MaxFrameBytes);
        CHECK(rtype::voice::isValidFrame(encoded.data()));
    }
}

TEST_CASE("Silence encodes to frames that need not be sent", "[voice]")
{
    Encoder encoder;
    const Frame silence{};
    // Opus waits 200 ms of silence, 10 frames, before it stops sending; then it gives one frame every 400 ms,
    // an update of its noise, which is not sent either
    for (int i = 0; i < 10; ++i) {
        (void)encoder.encode(silence);
    }
    std::size_t sent = 0;
    for (int i = 0; i < 40; ++i) {
        sent += encoder.encode(silence).silence() ? 0 : 1;
    }
    CHECK(sent == 0);

    Encoder continuous({.discontinuous = false});
    CHECK_FALSE(continuous.encode(silence).silence());
}

TEST_CASE("A lost frame is rebuilt from the copy the next one carries", "[voice]")
{
    const std::vector<Frame> original = voice(30);
    constexpr std::size_t Lost = 20;

    Encoder encoder;
    const std::vector<EncodedFrame> encoded = encodeAll(encoder, original);
    // Three decoders that heard the same frames before the lost one
    Decoder reference;
    Decoder rebuilt;
    Decoder concealed;
    Frame scratch;
    for (std::size_t frame = 0; frame < Lost; ++frame) {
        REQUIRE(reference.decode(encoded[frame].data(), scratch));
        REQUIRE(rebuilt.decode(encoded[frame].data(), scratch));
        REQUIRE(concealed.decode(encoded[frame].data(), scratch));
    }
    Frame heard;
    Frame fromFec;
    Frame madeUp;
    REQUIRE(reference.decode(encoded[Lost].data(), heard));
    REQUIRE(rebuilt.decodeFec(encoded[Lost + 1].data(), fromFec));
    concealed.conceal(madeUp);

    CHECK(fromFec != madeUp);
    CHECK(distance(fromFec, heard) < distance(madeUp, heard));
    // The next frame is decoded as usual after its copy was used
    CHECK(rebuilt.decode(encoded[Lost + 1].data(), scratch));
}

TEST_CASE("Without an expected loss, a lost frame is only concealed", "[voice]")
{
    const std::vector<Frame> original = voice(30);
    Encoder encoder({.expectedLoss = 0});
    const std::vector<EncodedFrame> encoded = encodeAll(encoder, original);
    Decoder fec;
    Decoder concealed;
    Frame scratch;
    for (std::size_t frame = 0; frame < 20; ++frame) {
        REQUIRE(fec.decode(encoded[frame].data(), scratch));
        REQUIRE(concealed.decode(encoded[frame].data(), scratch));
    }
    Frame fromFec;
    Frame madeUp;
    REQUIRE(fec.decodeFec(encoded[21].data(), fromFec));
    concealed.conceal(madeUp);
    CHECK(fromFec == madeUp);
}

TEST_CASE("The decoder refuses what is not a frame of 20 ms of the right size", "[voice]")
{
    Decoder decoder;
    Frame samples;
    samples.fill(1234);

    const std::vector<std::uint8_t> empty;
    const std::vector<std::uint8_t> tooBig(MaxFrameBytes + 1, 0xF8);
    // The first byte of an Opus packet tells its length: 0xF0, CELT fullband of 10 ms; 0xF8, of 20 ms
    const std::vector<std::uint8_t> tenMilliseconds{0xF0};
    const std::vector<std::uint8_t> twentyMilliseconds{0xF8};

    CHECK_FALSE(decoder.decode(empty, samples));
    CHECK_FALSE(decoder.decode(tooBig, samples));
    CHECK_FALSE(decoder.decode(tenMilliseconds, samples));
    CHECK_FALSE(decoder.decodeFec(tenMilliseconds, samples));
    for (const std::int16_t value : samples) {
        REQUIRE(value == 1234);
    }

    CHECK(rtype::voice::isValidFrame(twentyMilliseconds));
    CHECK(decoder.decode(twentyMilliseconds, samples));
}

TEST_CASE("The decoder survives random bytes", "[voice]")
{
    Decoder decoder;
    std::mt19937 random(7);
    std::uniform_int_distribution<std::size_t> size(1, MaxFrameBytes);
    std::uniform_int_distribution<int> byte(0, 255);
    Frame samples;
    std::size_t accepted = 0;
    for (int i = 0; i < 20000; ++i) {
        std::vector<std::uint8_t> frame(size(random));
        for (std::uint8_t &value : frame) {
            value = static_cast<std::uint8_t>(byte(random));
        }
        accepted += decoder.decode(frame, samples) ? 1 : 0;
        if (i % 7 == 0) {
            decoder.conceal(samples);
        }
    }
    // Some random packets are valid Opus of 20 ms: decoded as noise, never a crash
    CHECK(accepted < 20000);
}
