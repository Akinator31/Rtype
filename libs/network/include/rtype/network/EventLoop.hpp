#pragma once

#include <cstddef>
#include <functional>
#include <memory>

namespace rtype::network
{

namespace detail
{
struct EventLoopAccess;
}

/// Runs the network: waits for data, connections and the end of sends, and calls the handlers given to the
/// sockets.
///
/// Every socket belongs to one loop, and its handlers are only called from the thread that runs the loop,
/// with run() or poll():
///
/// - a **network thread** calls run(), which waits for events until stop() is called;
/// - a **game loop** calls poll() once per frame, which handles what is ready and returns at once.
///
/// Sockets must be destroyed before their loop.
class EventLoop
{
private:
    friend struct detail::EventLoopAccess;

    struct Impl;
    std::unique_ptr<Impl> _impl;

public:
    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop &) = delete;
    EventLoop &operator=(const EventLoop &) = delete;
    EventLoop(EventLoop &&) = delete;
    EventLoop &operator=(EventLoop &&) = delete;

    /// Handles events until stop() is called, waiting when there is nothing to do. Returns at once if the
    /// loop is already stopped.
    ///
    /// An exception thrown by a handler leaves run() and is given to its caller; calling run() again resumes
    /// the loop.
    void run();

    /// Handles the events that are ready, without waiting, then returns.
    /// @return The number of handlers called.
    std::size_t poll();

    /// Stops the loop: run() returns as soon as the current handler ends, and later calls to run() and poll()
    /// do nothing. Can be called from any thread.
    void stop();

    /// Tells whether stop() was called.
    [[nodiscard]] bool stopped() const;

    /// Calls a function on the thread that runs the loop, after the handlers already waiting. Can be called
    /// from any thread: this is how another thread hands work to the network thread.
    void post(std::function<void()> function);
};

} // namespace rtype::network
