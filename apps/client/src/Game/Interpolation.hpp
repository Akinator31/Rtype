#pragma once

#include "rtype/game/Constants.hpp"
#include "rtype/game/Snapshot.hpp"

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <vector>

namespace rtype::client
{

/// Draws a world received in snapshots smoothly (docs/protocol/game.md, section 9.3, "Drawing"): the
/// snapshots come 30 times per second, late or lost sometimes, but the client draws at its frame rate.
///
/// It keeps the last worlds received, each with its tick, and draws the world a little in the past, at the
/// **drawn tick**: delay() behind the newest world, between the two worlds around it.
///
/// @code
/// interpolation.receive(world.tick(), entities, players); // a snapshot applied
/// interpolation.destroyed(id, tick);                      // ENTITIES_DESTROYED
/// interpolation.advance(elapsed);                         // each frame
/// const rtype::game::Snapshot drawn = interpolation.view();
/// @endcode
///
/// - **The delay** follows the network: as short as the snapshots allow, so that the world is drawn as
///   recent as it can. Each world that arrives tells the delay it needed not to be waited for: the ticks
///   since the previous one (a lost snapshot makes it 4), plus how much later than the earliest of the last
///   Window seconds it arrived (the jitter), plus a tick of margin. The delay is the largest of the last
///   Window seconds, from MinDelay to MaxDelay; DefaultDelay until MinArrivals worlds came.
/// - **The drawn tick** advances with the time of the frames, 60 ticks per second, and is nudged towards
///   its target (the newest tick minus the delay): a little faster when it falls behind, a little slower when
///   it gets ahead, never backwards. The jitter of the snapshots, the drift between the clocks and a change
///   of the delay are absorbed without a jump; after a pause, too far behind, it jumps to its target.
/// - **An entity** is drawn between its positions in the two worlds around the drawn tick. Past the newest
///   world (snapshots late or lost), it goes on with its velocity for MaxExtrapolation at most, then waits.
/// - It **appears** once the drawn tick reaches its first world, and **disappears** once it reaches its
///   destruction: not 100 ms early, when the message arrives.
///
/// Pure: no clock, no network. The time is given by advance().
class Interpolation
{
public:
    /// How far in the past the world is drawn before the network is measured: 3 snapshots at 30 per second.
    static constexpr double DefaultDelay = 0.1;
    /// The shortest delay: a snapshot every 2 ticks, and a tick of margin.
    static constexpr double MinDelay = 3.0 / 60.0;
    /// The longest delay, whatever the network does.
    static constexpr double MaxDelay = 0.25;
    /// Seconds of arrivals the delay is measured over: a spike counts for that long.
    static constexpr double Window = 3.0;
    /// Worlds received before the delay is measured.
    static constexpr std::size_t MinArrivals = 8;
    /// How long an entity goes on with its velocity past the newest world.
    static constexpr double MaxExtrapolation = 0.25;
    /// How fast the drawn tick catches up with its target: this fraction of the gap per second.
    static constexpr double Correction = 2.0;
    /// Delay behind the target, in seconds, beyond which the drawn tick jumps to it.
    static constexpr double MaxGap = 0.5;
    /// Worlds kept: one second of snapshots, more than the delay needs.
    static constexpr std::size_t MaxWorlds = 32;

private:
    struct World {
        std::uint32_t tick = 0;
        std::map<std::uint32_t, game::EntityState> entities;
        std::vector<game::PlayerState> players;
    };

    /// A world received: when, by the time of advance(), and how many ticks after the previous one.
    struct Arrival {
        double time = 0.0;
        std::uint32_t tick = 0;
        std::uint32_t gap = 0;
    };

    std::deque<World> _worlds;
    std::deque<Arrival> _arrivals;
    double _time = 0.0;
    /// Tick of destruction of the entities destroyed, until the drawn tick passes it.
    std::map<std::uint32_t, std::uint32_t> _destroyed;
    std::optional<double> _drawn;

    [[nodiscard]] double target() const;
    void arrived(std::uint32_t tick);

public:
    /// The whole world at a tick: the RemoteWorld after the parts of that tick. A tick older than the newest
    /// is ignored; the same tick again replaces it (its parts come one by one).
    void receive(std::uint32_t tick, const std::map<std::uint32_t, game::EntityState> &entities,
                 std::vector<game::PlayerState> players);

    /// An entity destroyed at a tick: it disappears when the drawn tick reaches it.
    void destroyed(std::uint32_t id, std::uint32_t tick);

    /// Time goes by: the drawn tick advances.
    /// @param elapsed Time since the previous frame, in seconds.
    void advance(double elapsed);

    /// The world at the drawn tick: positions interpolated, or extrapolated past the newest world. Its tick
    /// is the drawn tick, rounded down. Empty before the first world.
    [[nodiscard]] game::Snapshot view() const;

    /// How far in the past the world is drawn now, in seconds (MinDelay to MaxDelay).
    [[nodiscard]] double delay() const;

    /// The tick drawn, with its fraction: nothing before the first world.
    [[nodiscard]] std::optional<double> drawnTick() const { return _drawn; }
    /// The newest tick received, 0 before the first world.
    [[nodiscard]] std::uint32_t newestTick() const { return _worlds.empty() ? 0 : _worlds.back().tick; }
    /// Worlds kept.
    [[nodiscard]] std::size_t size() const { return _worlds.size(); }
};

} // namespace rtype::client
