#pragma once

#include <cstdint>
#include <deque>
#include <optional>

namespace rtype::client
{

/// Keeps the inputs of the player arriving at the game just before their tick, from what the game tells
/// (docs/protocol/game.md, section 9.2): each SNAPSHOT gives the **lead** of the INPUT messages received
/// since the previous one, the tick of their newest input minus the last tick the game simulated. 1 is just
/// in time, 0 or less too late (the game ignores them), more is input latency for nothing.
///
/// @code
/// clock.advance(elapsed);                      // each frame
/// pending += elapsed * clock.rate();           // one INPUT per tick of the clock, not of the frames
/// stamp += 1 + clock.takeJump();               // each INPUT sent
/// clock.receive(part.inputLead, rtt + 0.05);   // each snapshot that has a lead
/// @endcode
///
/// - **The pace**: the smallest lead of the last Window seconds is kept between TargetLead and TargetLead +
///   Tolerance by sending the inputs a little faster or slower than 60 per second. The stamps stay
///   consecutive: no input is lost, none is stamped twice, and a stamp too far ahead comes back down.
/// - **The jump**: inputs too late are lost until the stamps catch up, so they jump ahead at once, by what is
///   missing. The reports of the inputs sent before the jump are ignored until it reaches the game.
/// - Before the first report, it is not **synced**: the stamps are those the round-trip time estimates.
///
/// Pure: no clock, no network. The time is given by advance().
class InputClock
{
public:
    /// The smallest lead aimed at: one tick of margin over just in time.
    static constexpr int TargetLead = 2;
    /// Ticks of lead above the target that are not corrected: the jitter of the network.
    static constexpr int Tolerance = 1;
    /// Seconds of reports over which the smallest lead is taken: a late datagram among them counts.
    static constexpr double Window = 0.5;
    /// How much faster or slower the inputs are sent, per tick of lead away from what is aimed at.
    static constexpr double Gain = 0.03;
    /// The slowest pace, a fraction of 60 inputs per second: the lead goes down by 30 ticks per second.
    static constexpr double MinRate = 0.5;
    /// The fastest pace.
    static constexpr double MaxRate = 1.1;

private:
    struct Report {
        double time = 0.0;
        int lead = 0;
    };

    std::deque<Report> _reports;
    double _time = 0.0;
    /// Before this time, the reports are of inputs sent before the last jump.
    double _settled = 0.0;
    std::uint32_t _jump = 0;
    bool _synced = false;

public:
    /// Time goes by: the old reports are forgotten.
    void advance(double elapsed);

    /// The lead a snapshot reports.
    /// @param settle How long a jump takes to show in the reports: the round-trip time and a snapshot.
    void receive(int lead, double settle);

    /// How many inputs to send per tick of time: 1 when the lead is right.
    [[nodiscard]] double rate() const;

    /// Ticks to skip on the next stamp, at once, then 0.
    [[nodiscard]] std::uint32_t takeJump();

    /// A report came: the stamps follow it, no longer the estimate.
    [[nodiscard]] bool synced() const { return _synced; }

    /// The smallest lead of the last reports, nothing without one.
    [[nodiscard]] std::optional<int> lead() const;
};

} // namespace rtype::client
