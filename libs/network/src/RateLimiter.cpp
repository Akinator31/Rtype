#include "rtype/network/RateLimiter.hpp"

#include "NetworkLogger.hpp"

namespace rtype::network
{

using detail::logger;

RateLimiter::RateLimiter(std::size_t perSecond) : _perSecond(perSecond) {}

bool RateLimiter::allow(const Address &from, Clock::time_point now)
{
    if (now - _lastPrune >= PruneInterval) {
        _lastPrune = now;
        std::erase_if(_counters,
                      [&](const auto &entry) { return now - entry.second.windowStart >= PruneInterval; });
    }
    Counter &counter = _counters[from];
    if (counter.count == 0 || now - counter.windowStart >= std::chrono::seconds(1)) {
        counter.windowStart = now;
        counter.count = 0;
    }
    ++counter.count;
    if (counter.count == _perSecond + 1) {
        logger().debug("{} sends more than {} datagrams per second: the others are dropped", from,
                       _perSecond);
    }
    return counter.count <= _perSecond;
}

} // namespace rtype::network
