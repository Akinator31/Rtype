#pragma once

#include <chrono>

namespace rtype::server
{

/// How many frames of the voice of one player the game forwards (docs/protocol/game.md, section 9.4): a
/// token bucket, filled at Rate frames per second up to Burst. A client sends 50 frames per second while its
/// player talks; the margin lets the frames a late datagram held back pass, and drops a client that sends
/// more for long.
///
/// @code
/// if (player.voiceRate.allow(Clock::now())) {
///     forward(frame);
/// }
/// @endcode
///
/// Pure: the time is given.
class VoiceRate
{
public:
    using Clock = std::chrono::steady_clock;

    /// Frames per second, in the long run.
    static constexpr double Rate = 60.0;
    /// Frames in a burst: 200 ms of voice.
    static constexpr double Burst = 10.0;

private:
    double _tokens = Burst;
    Clock::time_point _last{};
    bool _started = false;

public:
    /// Tells whether the frame received now is forwarded, and counts it if so.
    [[nodiscard]] bool allow(Clock::time_point now);
};

} // namespace rtype::server
