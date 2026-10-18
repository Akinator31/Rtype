#pragma once

#include <chrono>
#include <functional>
#include <memory>

namespace rtype::network
{

class EventLoop;

/// Calls a function after a delay, or at regular intervals, on the thread of an event loop: for timeouts and
/// for the work a network thread does every tick.
///
/// @code
/// rtype::network::Timer tick(loop);
/// tick.startRepeating(std::chrono::milliseconds(16), [&] { host.update(Clock::now()); });
///
/// rtype::network::Timer timeout(loop);
/// timeout.start(std::chrono::seconds(10), [&] { connection.close(); });
/// timeout.cancel(); // the handler will not be called
/// @endcode
///
/// Every function can be called from any thread. The handler is called on the loop's thread, and never after
/// cancel() or the destruction of the timer on that thread, even if it was already due. From another thread,
/// a call already running may still end.
class Timer
{
public:
    using Clock = std::chrono::steady_clock;
    using Handler = std::function<void()>;

    struct Impl;

private:
    std::shared_ptr<Impl> _impl;

public:
    explicit Timer(EventLoop &loop);
    /// Cancels the timer.
    ~Timer();

    Timer(const Timer &) = delete;
    Timer &operator=(const Timer &) = delete;
    Timer(Timer &&) noexcept;
    Timer &operator=(Timer &&) noexcept;

    /// Calls the handler once, after the delay. Replaces what the timer was doing.
    void start(Clock::duration delay, Handler handler);

    /// Calls the handler every interval, the first time one interval from now, until cancel(). Replaces what
    /// the timer was doing. If the loop is late, the calls missed are skipped rather than made in a burst.
    void startRepeating(Clock::duration interval, Handler handler);

    /// Stops the timer: the handler is no longer called.
    void cancel();
};

/// Calls a function when the program is asked to stop: Ctrl+C (SIGINT), SIGTERM (sent by `kill`, systemd or
/// Docker), and on Windows Ctrl+Break and the closing of the console (SIGBREAK). Each signal calls it again.
///
/// @code
/// rtype::network::StopSignals signals(loop, [&](std::string_view signal) {
///     logger.info("{} received: stopping", signal);
///     loop.stop();
/// });
/// loop.run();
/// @endcode
///
/// While it exists, these signals no longer end the program at once: the handler decides. The handler is
/// called on the loop's thread. Destroy it on that thread, or once the loop stopped.
class StopSignals
{
public:
    /// Called with the name of the signal, "SIGINT" for instance.
    using Handler = std::function<void(const char *signal)>;

    struct Impl;

private:
    std::shared_ptr<Impl> _impl;

public:
    StopSignals(EventLoop &loop, Handler handler);
    /// Gives the signals back to their default behaviour.
    ~StopSignals();

    StopSignals(const StopSignals &) = delete;
    StopSignals &operator=(const StopSignals &) = delete;
};

} // namespace rtype::network
