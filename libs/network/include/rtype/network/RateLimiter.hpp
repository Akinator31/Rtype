#pragma once

#include "rtype/network/Address.hpp"

#include <chrono>
#include <cstddef>
#include <unordered_map>

namespace rtype::network
{

/// Counts the datagrams of each source address, in windows of one second, and refuses those beyond a limit:
/// what a game, or the relay in front of it, checks before it decrypts or forwards anything
/// (docs/protocol/game.md, section 4).
///
/// @code
/// rtype::network::RateLimiter rates(rtype::network::MaxDatagramsPerSecond);
/// if (!rates.allow(from, Clock::now())) return; // dropped
/// @endcode
///
/// The counters of the addresses that stopped sending are forgotten after a while, so that a flood of
/// spoofed addresses does not grow it for ever. No clock of its own: it is given the time, as the sessions.
/// Used from one thread at a time.
class RateLimiter
{
public:
    using Clock = std::chrono::steady_clock;

    /// How long the counter of an address that stopped sending is kept.
    static constexpr std::chrono::seconds PruneInterval{10};

private:
    struct Counter {
        Clock::time_point windowStart;
        std::size_t count = 0;
    };

    std::size_t _perSecond;
    std::unordered_map<Address, Counter> _counters;
    Clock::time_point _lastPrune;

public:
    /// @param perSecond The datagrams accepted from one address in a second.
    explicit RateLimiter(std::size_t perSecond);

    /// Counts a datagram from an address.
    /// @return false if it is over the limit of its address in the current second: drop it.
    [[nodiscard]] bool allow(const Address &from, Clock::time_point now);

    /// The addresses counted now: for the tests.
    [[nodiscard]] std::size_t tracked() const { return _counters.size(); }
};

} // namespace rtype::network
