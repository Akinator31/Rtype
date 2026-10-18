#include "rtype/network/Timer.hpp"

#include "Asio.hpp"

#include <atomic>
#include <csignal>
#include <cstdint>

namespace rtype::network
{

using detail::logger;

// --- Timer ---

// Owned by the timer and by its pending wait. Each start() or cancel() changes the generation, at once and
// from any thread: a wait of an older generation finds it changed, and calls nothing.
struct Timer::Impl : std::enable_shared_from_this<Impl> {
    asio::steady_timer timer;
    std::atomic<std::uint64_t> generation = 0;
    // Used on the loop's thread only
    Handler handler;
    Clock::duration interval{};
    bool repeating = false;

    explicit Impl(asio::io_context &context) : timer(context) {}

    void arm(Clock::time_point when, std::uint64_t armed)
    {
        timer.expires_at(when);
        timer.async_wait(
            [self = shared_from_this(), armed](const std::error_code &error) { self->fired(error, armed); });
    }

    void fired(const std::error_code &error, std::uint64_t armed)
    {
        if (error || armed != generation) {
            return;
        }
        if (repeating) {
            Clock::time_point next = timer.expiry() + interval;
            const Clock::time_point now = Clock::now();
            if (next <= now) {
                // Late: skips the calls missed instead of making them all at once
                next = now + interval;
            }
            arm(next, armed);
        }
        // A copy: the handler may start or cancel the timer
        const Handler call = handler;
        if (call) {
            call();
        }
    }
};

Timer::Timer(EventLoop &loop) : _impl(std::make_shared<Impl>(detail::EventLoopAccess::context(loop))) {}

Timer::~Timer()
{
    cancel();
}

Timer::Timer(Timer &&) noexcept = default;

Timer &Timer::operator=(Timer &&other) noexcept
{
    if (this != &other) {
        cancel();
        _impl = std::move(other._impl);
    }
    return *this;
}

void Timer::start(Clock::duration delay, Handler handler)
{
    if (!_impl) {
        return;
    }
    const std::uint64_t armed = ++_impl->generation;
    asio::post(_impl->timer.get_executor(),
               [impl = _impl, armed, delay, handler = std::move(handler)]() mutable {
                   if (armed != impl->generation) {
                       return;
                   }
                   impl->handler = std::move(handler);
                   impl->repeating = false;
                   impl->arm(Clock::now() + delay, armed);
               });
}

void Timer::startRepeating(Clock::duration interval, Handler handler)
{
    if (!_impl) {
        return;
    }
    const std::uint64_t armed = ++_impl->generation;
    asio::post(_impl->timer.get_executor(),
               [impl = _impl, armed, interval, handler = std::move(handler)]() mutable {
                   if (armed != impl->generation) {
                       return;
                   }
                   impl->handler = std::move(handler);
                   impl->repeating = true;
                   impl->interval = interval;
                   impl->arm(Clock::now() + interval, armed);
               });
}

void Timer::cancel()
{
    if (!_impl) {
        return;
    }
    ++_impl->generation;
    asio::post(_impl->timer.get_executor(), [impl = _impl] {
        impl->timer.cancel();
        impl->handler = nullptr;
    });
}

// --- StopSignals ---

struct StopSignals::Impl : std::enable_shared_from_this<Impl> {
    asio::signal_set signals;
    Handler handler;
    std::atomic<bool> stopped = false;

    explicit Impl(asio::io_context &context) : signals(context) {}

    void waitNext()
    {
        signals.async_wait([self = shared_from_this()](const std::error_code &error, int number) {
            if (error || self->stopped) {
                return;
            }
            const char *name = "a stop signal";
            switch (number) {
            case SIGINT:
                name = "SIGINT";
                break;
            case SIGTERM:
                name = "SIGTERM";
                break;
#ifdef SIGBREAK
            case SIGBREAK:
                name = "SIGBREAK";
                break;
#endif
            default:
                break;
            }
            logger().debug("Received {}", name);
            self->waitNext();
            self->handler(name);
        });
    }
};

StopSignals::StopSignals(EventLoop &loop, Handler handler)
    : _impl(std::make_shared<Impl>(detail::EventLoopAccess::context(loop)))
{
    _impl->handler = std::move(handler);
    _impl->signals.add(SIGINT);
    _impl->signals.add(SIGTERM);
#ifdef SIGBREAK
    _impl->signals.add(SIGBREAK);
#endif
    _impl->waitNext();
}

StopSignals::~StopSignals()
{
    _impl->stopped = true;
    std::error_code ignored;
    // Removes the handlers of Asio: the signals end the program again
    _impl->signals.clear(ignored);
    _impl->signals.cancel(ignored);
}

} // namespace rtype::network
