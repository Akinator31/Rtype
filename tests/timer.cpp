// Tests of the timers and stop signals of libs/network, on a real event loop.

#include "rtype/network/Timer.hpp"

#include "rtype/network/EventLoop.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <string>
#include <thread>
#include <vector>

using namespace rtype::network;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

namespace
{

template <typename Condition>
bool pollUntil(EventLoop &loop, Condition condition,
               std::chrono::milliseconds timeout = std::chrono::seconds(5))
{
    const auto deadline = Clock::now() + timeout;
    while (!condition()) {
        if (Clock::now() > deadline) {
            return false;
        }
        if (loop.poll() == 0) {
            std::this_thread::sleep_for(1ms);
        }
    }
    return true;
}

} // namespace

TEST_CASE("A timer calls its handler once, after the delay", "[network]")
{
    EventLoop loop;
    Timer timer(loop);
    int calls = 0;
    const auto start = Clock::now();

    timer.start(50ms, [&] { ++calls; });

    REQUIRE(pollUntil(loop, [&] { return calls == 1; }));
    CHECK(Clock::now() - start >= 50ms);
    pollUntil(loop, [] { return false; }, 100ms);
    CHECK(calls == 1);
}

TEST_CASE("A repeating timer calls its handler until cancelled", "[network]")
{
    EventLoop loop;
    Timer timer(loop);
    int calls = 0;

    timer.startRepeating(10ms, [&] {
        if (++calls == 5) {
            timer.cancel();
        }
    });

    REQUIRE(pollUntil(loop, [&] { return calls == 5; }));
    pollUntil(loop, [] { return false; }, 100ms);
    CHECK(calls == 5);
}

TEST_CASE("A cancelled or destroyed timer never calls its handler, even when already due", "[network]")
{
    EventLoop loop;
    int calls = 0;
    Timer cancelled(loop);
    cancelled.start(10ms, [&] { ++calls; });
    {
        Timer destroyed(loop);
        destroyed.start(10ms, [&] { ++calls; });
        // Due, but the loop has not run yet
        std::this_thread::sleep_for(30ms);
        cancelled.cancel();
    }

    pollUntil(loop, [] { return false; }, 100ms);
    CHECK(calls == 0);
}

TEST_CASE("A timer started again forgets the previous handler", "[network]")
{
    EventLoop loop;
    Timer timer(loop);
    std::vector<std::string> calls;

    timer.start(10ms, [&] { calls.emplace_back("first"); });
    timer.start(30ms, [&] { calls.emplace_back("second"); });

    REQUIRE(pollUntil(loop, [&] { return !calls.empty(); }));
    pollUntil(loop, [] { return false; }, 100ms);
    CHECK(calls == std::vector<std::string>{"second"});
}

TEST_CASE("A timer can be started from another thread", "[network]")
{
    EventLoop loop;
    Timer timer(loop);
    std::atomic<std::thread::id> calledOn;

    std::thread other([&] { timer.start(10ms, [&] { calledOn = std::this_thread::get_id(); }); });
    other.join();

    REQUIRE(pollUntil(loop, [&] { return calledOn.load() != std::thread::id(); }));
    CHECK(calledOn.load() == std::this_thread::get_id());
}

TEST_CASE("Stop signals call their handler on the loop's thread, each time", "[network]")
{
    EventLoop loop;
    std::vector<std::string> received;
    const StopSignals signals(loop, [&](const char *signal) { received.emplace_back(signal); });

    std::raise(SIGINT);
    std::raise(SIGTERM);

    REQUIRE(pollUntil(loop, [&] { return received.size() == 2; }));
    CHECK(received == std::vector<std::string>{"SIGINT", "SIGTERM"});
}
