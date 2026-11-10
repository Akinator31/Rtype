#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

/// The voice of one player as it arrives, put back in order and in time (docs/architecture/voice.md).
namespace rtype::voice
{

/// Puts the frames of the voice of one player back in their order and plays them at their pace, one every
/// 20 ms, a little after they arrive: late frames are heard, not lost.
///
/// - **A talk spurt** starts when a frame comes while nothing plays: it is played from a delay after it, 40
/// ms
///   at first. The delay grows by 20 ms, up to 120 ms, at each frame that comes too late, and shrinks by
///   20 ms, down to 40 ms, after 5 seconds without one.
/// - **A missing frame** is rebuilt from the copy the next one carries if it is there (Kind::Fec), made up
///   otherwise (Kind::Conceal). After 5 of them in a row, or after the last frame of a spurt, it stops until
///   the next spurt.
/// - A frame received twice, or from a spurt that is over, is dropped.
///
/// @code
/// buffer.push({.sequence = frame.sequence, .last = frame.last, .bytes = frame.bytes}, Clock::now());
/// // Each frame of the game loop
/// while (const auto out = buffer.pop(Clock::now())) {
///     decode(*out);    // 20 ms of voice
/// }
/// @endcode
///
/// Pure: it decodes nothing and reads no clock, the time is given.
class JitterBuffer
{
public:
    using Clock = std::chrono::steady_clock;

    /// The time of one frame.
    static constexpr std::chrono::milliseconds FrameTime{20};
    static constexpr std::chrono::milliseconds MinDelay{40};
    static constexpr std::chrono::milliseconds MaxDelay{120};
    static constexpr std::chrono::milliseconds DelayStep{20};
    /// Frames played without a late one before the delay shrinks: 5 seconds.
    static constexpr std::size_t SteadyFrames = 250;
    /// Frames missing in a row made up before it stops.
    static constexpr std::size_t MaxConcealed = 5;
    /// Frames kept ahead of the one to play: one second. A frame further ahead starts a new spurt.
    static constexpr std::size_t Capacity = 50;

    /// A frame as the game gives it.
    struct Frame {
        std::uint16_t sequence = 0;
        /// The last of a talk spurt.
        bool last = false;
        std::vector<std::uint8_t> bytes;
    };

    enum class Kind : std::uint8_t {
        /// The frame to play: decode bytes.
        Frame,
        /// It is missing, the next one is there: bytes is the next one, decodeFec() it.
        Fec,
        /// It is missing and so is the next one: conceal(), bytes is empty.
        Conceal,
    };

    /// 20 ms to play.
    struct Output {
        Kind kind = Kind::Frame;
        std::vector<std::uint8_t> bytes;
    };

    /// A frame received at `arrival`.
    void push(Frame frame, Clock::time_point arrival);
    /// The next 20 ms if their time came by `now`; nothing otherwise, or while no spurt plays. Called until
    /// it gives nothing, each frame of the game loop. Behind by more than MaxDelay (the game loop stalled),
    /// it drops the frames whose time passed, and plays on from the one of now.
    [[nodiscard]] std::optional<Output> pop(Clock::time_point now);
    /// Forgets everything: the delay goes back to MinDelay.
    void reset();

    /// A spurt plays, or is about to.
    [[nodiscard]] bool playing() const { return _playing; }
    /// The delay a spurt starts with now.
    [[nodiscard]] std::chrono::milliseconds delay() const { return _delay; }
    /// Frames received and not played yet.
    [[nodiscard]] std::size_t buffered() const { return _frames.size(); }
    /// Frames that came after their time, since the start.
    [[nodiscard]] std::size_t late() const { return _late; }

private:
    /// The frames by sequence, unwrapped: a sequence of 16 bits goes around in 22 minutes.
    std::map<std::int64_t, Frame> _frames;
    /// The newest sequence seen, unwrapped, to unwrap the next ones.
    std::optional<std::int64_t> _newest;
    /// The last sequence played or made up, unwrapped: anything before is too late.
    std::optional<std::int64_t> _played;
    /// The frames of this spurt made up, the newest last: one of them coming after is late.
    std::vector<std::int64_t> _missed;
    bool _playing = false;
    /// The first frame of the spurt is played.
    bool _started = false;
    std::int64_t _next = 0;
    Clock::time_point _playAt{};
    std::chrono::milliseconds _delay = MinDelay;
    std::size_t _concealed = 0;
    std::size_t _steady = 0;
    std::size_t _late = 0;

    [[nodiscard]] std::int64_t unwrap(std::uint16_t sequence) const;
    void start(Clock::time_point at);
    /// The spurt is over: the next frames start another, at once if some are already there.
    void stop();
    void arrivedLate();
};

} // namespace rtype::voice
