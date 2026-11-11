// Tests of the pace of the inputs online (InputClock): alone, then against the InputBuffer of the server,
// with a clock of the client that drifts, a network that delays, and a game that falls behind
// (docs/protocol/game.md, section 9.2).

#include "Game/InputClock.hpp"
#include "Instance/InputBuffer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

using rtype::client::InputClock;
using rtype::game::PlayerInput;
using rtype::server::InputBuffer;

namespace
{

constexpr double Tick = 1.0 / 60.0;

/// A client and the game, at 240 frames per second: the client paces and stamps its inputs with an
/// InputClock, they reach the game `up` seconds later, the leads come back `down` seconds later, every 2
/// ticks.
struct Link {
    InputBuffer buffer;
    InputClock clock;

    /// How fast the time of the client goes, against the one of the game: 1.01 is 1% fast.
    double drift = 1.0;
    double up = 0.03;
    double down = 0.03;
    /// Up to this much more for each INPUT, at random: they may arrive out of order.
    double jitter = 0.0;
    /// Every this many INPUT, one held back `spike` seconds more: a network that hiccups now and then.
    std::uint32_t spikeEvery = 0;
    double spike = 0.0;
    std::uint32_t sentCount = 0;
    std::uint32_t random = 12345;
    /// The first stamp, ahead of the game: what the estimate gave.
    std::uint32_t firstLead = 3;

    double now = 0.0;
    double nextTick = Tick;
    double pending = 0.0;
    std::uint32_t simulated = 0;
    std::uint32_t stamp = 0;
    std::optional<std::int64_t> lead;

    struct Sent {
        double arrives;
        std::uint32_t stamp;
    };
    struct Report {
        double arrives;
        std::int64_t lead;
    };
    std::deque<Sent> inputs;
    std::deque<Report> reports;

    /// The ticks the game applied the input stamped for them, and those it did not have after the first.
    std::uint32_t onTime = 0;
    std::uint32_t missed = 0;
    /// The leads reported, as the client sees them.
    std::vector<std::int64_t> leads;

    /// Runs for `seconds`. `stall` stops the game for this long at the start, as a loaded machine does.
    void run(double seconds, double stall = 0.0)
    {
        const double end = now + seconds;
        const double frame = 1.0 / 240.0;
        nextTick += stall;
        while (now < end) {
            now += frame;
            client(frame * drift);
            while (now >= nextTick) {
                game();
                nextTick += Tick;
            }
            while (!reports.empty() && reports.front().arrives <= now) {
                leads.push_back(reports.front().lead);
                clock.receive(static_cast<int>(reports.front().lead), up + down + (2 * Tick));
                reports.pop_front();
            }
        }
    }

    void client(double elapsed)
    {
        clock.advance(elapsed);
        pending += elapsed * clock.rate();
        while (pending >= Tick) {
            pending -= Tick;
            stamp = stamp == 0 ? simulated + firstLead : stamp + 1;
            stamp += clock.takeJump();
            random = (random * 1103515245U) + 12345U;
            double late = jitter * static_cast<double>((random >> 16) % 1000) / 1000.0;
            if (spikeEvery != 0 && ++sentCount % spikeEvery == 0) {
                late += spike;
            }
            const Sent sent{now + up + late, stamp};
            inputs.insert(std::ranges::upper_bound(inputs, sent.arrives, {}, &Sent::arrives), sent);
        }
    }

    void game()
    {
        while (!inputs.empty() && inputs.front().arrives <= now) {
            buffer.receive(inputs.front().stamp, std::vector{PlayerInput{}}, simulated);
            inputs.pop_front();
        }
        if (const std::optional<std::int64_t> received = buffer.takeLead()) {
            lead = lead ? std::min(*lead, *received) : *received;
        }
        ++simulated;
        (void)buffer.take(simulated);
        if (buffer.lastApplied() == simulated) {
            ++onTime;
        } else if (onTime > 0) {
            // Before the first input, there was nothing to miss
            ++missed;
        }
        if (simulated % 2 == 0 && lead) {
            reports.push_back({now + down, *lead});
            lead.reset();
        }
    }

    /// The leads reported over the last `count` reports.
    [[nodiscard]] std::vector<std::int64_t> lastLeads(std::size_t count) const
    {
        const std::size_t first = leads.size() > count ? leads.size() - count : 0;
        return {leads.begin() + static_cast<std::ptrdiff_t>(first), leads.end()};
    }
};

std::int64_t lowest(const std::vector<std::int64_t> &values)
{
    return std::ranges::min(values);
}

std::int64_t highest(const std::vector<std::int64_t> &values)
{
    return std::ranges::max(values);
}

} // namespace

TEST_CASE("Without a report, the inputs go at 60 per second and the estimate stamps them", "[input-clock]")
{
    InputClock clock;
    CHECK_FALSE(clock.synced());
    CHECK(clock.rate() == 1.0);
    CHECK(clock.takeJump() == 0);
    CHECK_FALSE(clock.lead());
}

TEST_CASE("A lead within the target leaves the pace alone", "[input-clock]")
{
    InputClock clock;
    clock.receive(InputClock::TargetLead, 0.1);
    CHECK(clock.synced());
    CHECK(clock.rate() == 1.0);
    clock.receive(InputClock::TargetLead + InputClock::Tolerance, 0.1);
    CHECK(clock.lead() == InputClock::TargetLead);
    CHECK(clock.rate() == 1.0);
}

TEST_CASE("Inputs too early slow down, the further the slower, never below half", "[input-clock]")
{
    InputClock clock;
    clock.receive(InputClock::TargetLead + InputClock::Tolerance + 2, 0.1);
    const double slightly = clock.rate();
    CHECK(slightly < 1.0);

    clock.advance(InputClock::Window + 0.01);
    clock.receive(InputClock::TargetLead + 50, 0.1);
    CHECK(clock.rate() < slightly);
    CHECK(clock.rate() == InputClock::MinRate);
    // Never a jump back: the stamps stay consecutive
    CHECK(clock.takeJump() == 0);
}

TEST_CASE("Inputs just in time speed up a little", "[input-clock]")
{
    InputClock clock;
    clock.receive(1, 0.1);
    CHECK(clock.rate() > 1.0);
    CHECK(clock.rate() <= InputClock::MaxRate);
}

TEST_CASE("The smallest lead of the last reports counts: one late datagram is enough", "[input-clock]")
{
    InputClock clock;
    clock.receive(1, 0.1);
    clock.advance(0.1);
    clock.receive(8, 0.1);
    CHECK(clock.lead() == 1);
    CHECK(clock.rate() > 1.0);

    // Forgotten once out of the window
    clock.advance(InputClock::Window);
    CHECK(clock.lead() == 8);
    CHECK(clock.rate() < 1.0);
}

TEST_CASE("Inputs too late jump at once, and the reports of before the jump are ignored", "[input-clock]")
{
    InputClock clock;
    clock.receive(-3, 0.2);
    CHECK(clock.takeJump() == static_cast<std::uint32_t>(InputClock::TargetLead + 3));
    CHECK(clock.takeJump() == 0);

    // Still the inputs of before the jump: no second jump
    clock.advance(0.1);
    clock.receive(-2, 0.2);
    CHECK(clock.takeJump() == 0);
    CHECK_FALSE(clock.lead());

    // The jump reached the game
    clock.advance(0.15);
    clock.receive(2, 0.2);
    CHECK(clock.lead() == 2);
    clock.receive(0, 0.2);
    // Late after the first report: a tick of margin more
    CHECK(clock.target() == InputClock::TargetLead + 1);
    CHECK(clock.takeJump() == static_cast<std::uint32_t>(InputClock::TargetLead + 1));
}

TEST_CASE("Inputs too late raise the margin, a calm network lowers it", "[input-clock]")
{
    InputClock clock;
    clock.receive(3, 0.0);
    CHECK(clock.target() == InputClock::TargetLead);

    for (int late = 1; late <= InputClock::MaxMargin + 2; ++late) {
        clock.receive(0, 0.0);
        CHECK(clock.target() == InputClock::TargetLead + std::min(late, InputClock::MaxMargin));
    }
    (void)clock.takeJump();

    // A tick back at each calm period
    clock.advance(InputClock::Calm - 0.1);
    CHECK(clock.target() == InputClock::TargetLead + InputClock::MaxMargin);
    clock.advance(0.2);
    CHECK(clock.target() == InputClock::TargetLead + InputClock::MaxMargin - 1);
    for (int calm = 0; calm < InputClock::MaxMargin; ++calm) {
        clock.advance(InputClock::Calm);
    }
    CHECK(clock.target() == InputClock::TargetLead);
}

TEST_CASE("A clock of the client 1% fast keeps its inputs just in time for minutes", "[input-clock]")
{
    // The stamps of before only went up: after about 100 seconds, every input was more than 60 ticks ahead,
    // and the game ignored them all
    Link link;
    link.drift = 1.01;
    link.run(300.0);

    CHECK(link.missed == 0);
    const std::vector<std::int64_t> last = link.lastLeads(300);
    CHECK(lowest(last) >= 1);
    CHECK(highest(last) <= InputClock::TargetLead + InputClock::Tolerance + 2);
}

TEST_CASE("A clock of the client 1% slow keeps its inputs just in time for minutes", "[input-clock]")
{
    Link link;
    link.drift = 0.99;
    link.run(300.0);

    CHECK(link.missed == 0);
    const std::vector<std::int64_t> last = link.lastLeads(300);
    CHECK(highest(last) <= InputClock::TargetLead + InputClock::Tolerance + 2);
}

TEST_CASE("Stamps far ahead come back down without losing an input", "[input-clock]")
{
    Link link;
    link.firstLead = 50;
    link.run(10.0);

    CHECK(link.missed == 0);
    const std::vector<std::int64_t> last = link.lastLeads(60);
    CHECK(lowest(last) >= 1);
    CHECK(highest(last) <= InputClock::TargetLead + InputClock::Tolerance + 1);
}

TEST_CASE("A game that stalls, then skips the ticks it missed, is followed", "[input-clock]")
{
    Link link;
    link.run(5.0);
    // The game stops for 300 ms, as GameThread::MaxLag makes it go on from then: its stamps are 18 ticks
    // further than they should be
    link.run(10.0, 0.3);

    const std::vector<std::int64_t> last = link.lastLeads(60);
    CHECK(lowest(last) >= 1);
    CHECK(highest(last) <= InputClock::TargetLead + InputClock::Tolerance + 1);
}

TEST_CASE("A client that starts late jumps, then stays on time", "[input-clock]")
{
    Link link;
    link.firstLead = 0;
    link.up = 0.08;
    link.down = 0.08;
    link.run(10.0);

    const std::vector<std::int64_t> last = link.lastLeads(120);
    CHECK(lowest(last) >= 1);
    CHECK(highest(last) <= InputClock::TargetLead + InputClock::Tolerance + 1);
}

TEST_CASE("A network that jitters keeps the inputs just in time, with a margin for the late ones",
          "[input-clock]")
{
    Link link;
    link.drift = 1.005;
    link.jitter = 0.04;
    link.run(10.0);
    // The worst datagram sets the lead: a few late ones at the start, while it learns the jitter
    CHECK(link.missed <= 10);

    link.missed = 0;
    link.run(120.0);
    CHECK(link.missed == 0);
    const std::vector<std::int64_t> last = link.lastLeads(600);
    CHECK(lowest(last) >= 1);
    // Not much more than the jitter, 2.4 ticks, above the target
    CHECK(highest(last) <= InputClock::TargetLead + InputClock::Tolerance + 4);
}

TEST_CASE("A network that hiccups more rarely than the window stops losing inputs once the margin is there",
          "[input-clock]")
{
    // A datagram 70 ms late every 2 s: the smallest lead of the window forgets it before the next one
    Link link;
    link.up = 0.04;
    link.down = 0.04;
    link.spikeEvery = 120;
    link.spike = 0.07;
    link.run(30.0);
    CAPTURE(link.missed);
    CHECK(link.missed > 0);
    CHECK(link.clock.target() > InputClock::TargetLead);

    link.missed = 0;
    link.run(60.0);
    CHECK(link.missed == 0);
}
