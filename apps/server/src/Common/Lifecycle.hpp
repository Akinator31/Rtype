#pragma once

#include "rtype/network/EventLoop.hpp"
#include "rtype/network/Timer.hpp"

#include <chrono>
#include <functional>

namespace rtype::server
{

/// How long a server may take to stop properly after Ctrl+C or SIGTERM, before it stops at once.
inline constexpr std::chrono::seconds GracePeriod{10};

/// Runs the event loop on this thread until the server stops, and handles the stop signals:
///
/// - The first Ctrl+C or SIGTERM calls `shutdown`, which ends the work properly (tells the clients, waits for
///   the games...) and calls `loop.stop()` once done.
/// - A second signal, or the grace period without the loop stopped, stops the loop at once.
///
/// @code
/// const bool clean = runUntilStopped(loop, [&] {
///     listener.close();
///     loop.stop();
/// });
/// return clean ? Success : Failure;
/// @endcode
///
/// @return true if the server stopped properly: through `shutdown`, or because the loop was stopped by the
/// work itself. false if it was forced.
[[nodiscard]] bool runUntilStopped(rtype::network::EventLoop &loop, std::function<void()> shutdown,
                                   rtype::network::Timer::Clock::duration grace = GracePeriod);

} // namespace rtype::server
