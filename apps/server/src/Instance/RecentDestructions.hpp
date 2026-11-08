#pragma once

#include "rtype/game/Messages.hpp"

#include <cstdint>
#include <deque>
#include <vector>

namespace rtype::server
{

/// The ENTITIES_DESTROYED of the last ticks, sent again with each snapshot (docs/protocol/game.md,
/// section 9.3).
///
/// They are unreliable, as the inputs: sent at their tick, then again with every snapshot for Repeat ticks,
/// so that a lost datagram costs a snapshot interval, never a resend after a round trip, and never holds back
/// the messages behind it.
///
/// @code
/// destructions.add(tick, rtype::game::destroyedMessages(snapshot)); // each tick, sent at once besides
/// for (const auto &message : destructions.again(tick)) { ... }      // with each snapshot
/// @endcode
///
/// Pure: no clock, no network.
class RecentDestructions
{
public:
    /// Ticks a destruction is sent again for: 15 snapshots.
    static constexpr std::uint32_t Repeat = 30;

private:
    std::deque<rtype::game::EntitiesDestroyedMessage> _messages;

public:
    /// The destructions of a tick, already sent once. Forgets those older than Repeat ticks.
    void add(std::uint32_t tick, const std::vector<rtype::game::EntitiesDestroyedMessage> &messages);

    /// What to send again with the snapshot of `tick`: the destructions of the Repeat ticks before it, not
    /// those of `tick` itself, sent already.
    [[nodiscard]] std::vector<rtype::game::EntitiesDestroyedMessage> again(std::uint32_t tick) const;

    /// Messages kept.
    [[nodiscard]] std::size_t size() const { return _messages.size(); }
};

} // namespace rtype::server
