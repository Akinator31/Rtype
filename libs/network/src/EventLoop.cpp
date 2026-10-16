#include "rtype/network/EventLoop.hpp"

#include "Asio.hpp"

namespace rtype::network
{

struct EventLoop::Impl {
    asio::io_context context{1};
    // Keeps run() waiting while no socket has any work, until stop()
    asio::executor_work_guard<asio::io_context::executor_type> work = asio::make_work_guard(context);
};

EventLoop::EventLoop() : _impl(std::make_unique<Impl>()) {}

EventLoop::~EventLoop() = default;

void EventLoop::run()
{
    _impl->context.run();
}

std::size_t EventLoop::poll()
{
    return _impl->context.poll();
}

void EventLoop::stop()
{
    _impl->context.stop();
}

bool EventLoop::stopped() const
{
    return _impl->context.stopped();
}

void EventLoop::post(std::function<void()> function)
{
    asio::post(_impl->context, std::move(function));
}

namespace detail
{

asio::io_context &EventLoopAccess::context(EventLoop &loop)
{
    return loop._impl->context;
}

} // namespace detail

} // namespace rtype::network
