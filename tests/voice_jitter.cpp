#include "rtype/voice/JitterBuffer.hpp"
#include "rtype/voice/RingBuffer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using rtype::voice::JitterBuffer;
using Kind = JitterBuffer::Kind;

namespace
{

const JitterBuffer::Clock::time_point Start{};

/// A frame whose one byte is its sequence, to tell them apart
JitterBuffer::Frame frame(std::uint16_t sequence, bool last = false)
{
    return {sequence, last, {static_cast<std::uint8_t>(sequence)}};
}

/// What the buffer gives at each time from `from`, every 20 ms, `count` times: the byte of each frame
/// played, -1 for a frame rebuilt from the next one, -2 for one made up, -3 for nothing
std::vector<int> playFrom(JitterBuffer &buffer, JitterBuffer::Clock::time_point from, int count)
{
    std::vector<int> heard;
    for (int i = 0; i < count; ++i) {
        const std::optional<JitterBuffer::Output> out = buffer.pop(from + i * JitterBuffer::FrameTime);
        if (!out) {
            heard.push_back(-3);
        } else if (out->kind == Kind::Frame) {
            heard.push_back(out->bytes.at(0));
        } else {
            heard.push_back(out->kind == Kind::Fec ? -1 : -2);
        }
    }
    return heard;
}

} // namespace

TEST_CASE("A jitter buffer plays its frames in order, 40 ms after the first one came", "[voice]")
{
    JitterBuffer buffer;
    buffer.push(frame(10), Start);
    // Out of order, the next ones before the delay
    buffer.push(frame(12), Start + 5ms);
    buffer.push(frame(11), Start + 10ms);

    CHECK_FALSE(buffer.pop(Start + 39ms));
    CHECK(buffer.playing());
    CHECK(playFrom(buffer, Start + 40ms, 3) == std::vector<int>{10, 11, 12});
    CHECK(buffer.buffered() == 0);
}

TEST_CASE("A frame that came before the first one played is played first", "[voice]")
{
    JitterBuffer buffer;
    buffer.push(frame(11), Start);
    buffer.push(frame(10), Start + 15ms);

    CHECK(playFrom(buffer, Start + 40ms, 2) == std::vector<int>{10, 11});
}

TEST_CASE("A lost frame is rebuilt from the next one, or made up without it", "[voice]")
{
    JitterBuffer buffer;
    buffer.push(frame(1), Start);
    buffer.push(frame(3), Start);
    buffer.push(frame(6), Start);

    // 2 is lost and 3 is there: from 3; 4 and 5 lost, and nothing after 4: made up, then from 6
    const std::vector<int> heard = playFrom(buffer, Start + 40ms, 6);
    CHECK(heard == std::vector<int>{1, -1, 3, -2, -1, 6});

    // The copy of the next frame is a copy: the next one is played as usual
    JitterBuffer copy;
    copy.push(frame(1), Start);
    copy.push(frame(3), Start);
    (void)copy.pop(Start + 40ms);
    const auto rebuilt = copy.pop(Start + 60ms);
    REQUIRE(rebuilt);
    CHECK(rebuilt->kind == Kind::Fec);
    CHECK(rebuilt->bytes == std::vector<std::uint8_t>{3});
}

TEST_CASE("After 5 frames made up, or the last frame of a spurt, the buffer stops until the next spurt",
          "[voice]")
{
    JitterBuffer silent;
    silent.push(frame(1), Start);
    CHECK(playFrom(silent, Start + 40ms, 7) == std::vector<int>{1, -2, -2, -2, -2, -2, -3});
    CHECK_FALSE(silent.playing());

    JitterBuffer ended;
    ended.push(frame(1), Start);
    ended.push(frame(2, true), Start);
    CHECK(playFrom(ended, Start + 40ms, 3) == std::vector<int>{1, 2, -3});
    CHECK_FALSE(ended.playing());

    // The next spurt waits its delay again, from its first frame
    ended.push(frame(9), Start + 1s);
    CHECK_FALSE(ended.pop(Start + 1s + 20ms));
    CHECK(playFrom(ended, Start + 1s + 40ms, 1) == std::vector<int>{9});
}

TEST_CASE("A frame received twice, or of a spurt over, is dropped", "[voice]")
{
    JitterBuffer buffer;
    buffer.push(frame(1), Start);
    buffer.push(frame(1), Start + 1ms);
    buffer.push(frame(2, true), Start + 2ms);
    CHECK(buffer.buffered() == 2);
    CHECK(playFrom(buffer, Start + 40ms, 2) == std::vector<int>{1, 2});

    // Played, then received again: neither played nor late
    buffer.push(frame(1), Start + 100ms);
    buffer.push(frame(2), Start + 100ms);
    CHECK_FALSE(buffer.playing());
    CHECK(buffer.buffered() == 0);
    CHECK(buffer.late() == 0);
}

TEST_CASE("Frames that come late make the delay grow up to 120 ms, and steady ones make it shrink", "[voice]")
{
    JitterBuffer buffer;
    buffer.push(frame(0), Start);
    CHECK(buffer.delay() == JitterBuffer::MinDelay);
    // 1 never comes in time: made up, then it comes
    CHECK(playFrom(buffer, Start + 40ms, 2) == std::vector<int>{0, -2});
    buffer.push(frame(1), Start + 70ms);
    CHECK(buffer.late() == 1);
    CHECK(buffer.delay() == 60ms);

    // Each frame late raises it, never past the most
    auto now = Start + 80ms;
    for (std::uint16_t sequence = 2; sequence < 6; ++sequence) {
        // Made up, then it comes
        std::optional<JitterBuffer::Output> out;
        for (int i = 0; i < 20 && !(out && out->kind == Kind::Conceal); ++i) {
            out = buffer.pop(now);
            now += JitterBuffer::FrameTime;
        }
        buffer.push(frame(sequence), now);
    }
    CHECK(buffer.late() == 5);
    CHECK(buffer.delay() == JitterBuffer::MaxDelay);

    // 5 seconds of frames in time bring it down by a step
    JitterBuffer steady;
    steady.push(frame(0), Start);
    (void)playFrom(steady, Start + 40ms, 2);
    steady.push(frame(1), Start + 70ms);
    REQUIRE(steady.delay() == 60ms);
    auto at = Start + 100ms;
    for (std::uint16_t next = 2; next < 2 + JitterBuffer::SteadyFrames + 5; ++next) {
        steady.push(frame(next), at);
        while (steady.pop(at)) {
        }
        at += JitterBuffer::FrameTime;
    }
    CHECK(steady.delay() == JitterBuffer::MinDelay);
}

TEST_CASE("A game loop that stalls drops the voice whose time passed, instead of playing it late", "[voice]")
{
    JitterBuffer buffer;
    for (std::uint16_t sequence = 0; sequence < 40; ++sequence) {
        buffer.push(frame(sequence), Start + sequence * JitterBuffer::FrameTime);
    }
    // Half a second without a pop: the frames played from now on are those of now, give or take the delay
    const auto late = Start + 40ms + 500ms;
    const auto first = buffer.pop(late);
    REQUIRE(first);
    REQUIRE(first->kind == Kind::Frame);
    CHECK(first->bytes.at(0) >= 19);
    // Only what is due comes, not the whole backlog at once
    int given = 1;
    while (buffer.pop(late)) {
        ++given;
    }
    CHECK(given == 1);
}

TEST_CASE("Sequences go around from 65535 to 0, and a frame far ahead starts a new spurt", "[voice]")
{
    JitterBuffer buffer;
    buffer.push(frame(65534), Start);
    buffer.push(frame(0), Start);
    buffer.push(frame(65535), Start);
    const std::vector<int> heard = playFrom(buffer, Start + 40ms, 3);
    CHECK(heard == std::vector<int>{65534 % 256, 65535 % 256, 0});

    // The sender talked on for seconds while nothing came: its new frames start a spurt of their own
    buffer.push(frame(5000), Start + 100ms);
    CHECK(buffer.buffered() == 1);
    CHECK_FALSE(buffer.pop(Start + 120ms));
    CHECK(playFrom(buffer, Start + 140ms, 1) == std::vector<int>{5000 % 256});
}

TEST_CASE("A ring buffer gives the items in order, as many as it holds", "[voice]")
{
    rtype::voice::RingBuffer<int> ring(4);
    const std::array<int, 6> six{1, 2, 3, 4, 5, 6};
    CHECK(ring.write(six) == 4);
    CHECK(ring.size() == 4);

    std::array<int, 3> three{};
    CHECK(ring.read(three) == 3);
    CHECK(three == std::array<int, 3>{1, 2, 3});
    // Around the end of its storage
    CHECK(ring.write(std::array<int, 2>{7, 8}) == 2);
    std::array<int, 8> rest{};
    CHECK(ring.read(rest) == 3);
    CHECK(rest[0] == 4);
    CHECK(rest[1] == 7);
    CHECK(rest[2] == 8);

    CHECK(ring.write(std::array<int, 2>{9, 10}) == 2);
    ring.discard();
    CHECK(ring.size() == 0);
    CHECK(ring.read(rest) == 0);
}

TEST_CASE("A ring buffer carries every item from one thread to another, in order, without a lock", "[voice]")
{
    constexpr int Count = 200000;
    rtype::voice::RingBuffer<int> ring(97);
    std::atomic<bool> failed = false;
    std::thread producer([&] {
        int next = 0;
        while (next < Count) {
            std::array<int, 13> chunk{};
            int count = 0;
            for (; count < 13 && next + count < Count; ++count) {
                chunk[static_cast<std::size_t>(count)] = next + count;
            }
            next += static_cast<int>(
                ring.write(std::span<const int>(chunk.data(), static_cast<std::size_t>(count))));
        }
    });
    int expected = 0;
    while (expected < Count) {
        std::array<int, 7> chunk{};
        const std::size_t read = ring.read(chunk);
        for (std::size_t i = 0; i < read; ++i) {
            if (chunk[i] != expected++) {
                failed = true;
            }
        }
    }
    producer.join();
    CHECK_FALSE(failed);
    CHECK(ring.size() == 0);
}
