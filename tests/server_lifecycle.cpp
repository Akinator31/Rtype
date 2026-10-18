// Tests of how r-type_server stops: the stop signals, sent to the test process itself, and the grace period
// (docs/architecture/server.md).

#include "Common/Lifecycle.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/Timer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <csignal>

using namespace rtype::server;
using namespace std::chrono_literals;
using rtype::network::EventLoop;
using rtype::network::Timer;

TEST_CASE("The first stop signal calls the shutdown, which stops the server properly", "[server]")
{
    EventLoop loop;
    int shutdowns = 0;
    // Raised once the loop runs, when the signals are handled
    loop.post([] { std::raise(SIGINT); });

    const bool clean = runUntilStopped(loop, [&] {
        ++shutdowns;
        loop.stop();
    });

    CHECK(clean);
    CHECK(shutdowns == 1);
}

TEST_CASE("SIGTERM stops the server like Ctrl+C", "[server]")
{
    EventLoop loop;
    loop.post([] { std::raise(SIGTERM); });

    CHECK(runUntilStopped(loop, [&] { loop.stop(); }));
}

TEST_CASE("A second stop signal stops the server at once", "[server]")
{
    EventLoop loop;
    int shutdowns = 0;
    Timer second(loop);
    loop.post([] { std::raise(SIGINT); });

    // A shutdown that never ends, until the second Ctrl+C
    const bool clean = runUntilStopped(loop, [&] {
        ++shutdowns;
        second.start(20ms, [] { std::raise(SIGINT); });
    });

    CHECK_FALSE(clean);
    CHECK(shutdowns == 1);
}

TEST_CASE("A shutdown longer than the grace period is cut", "[server]")
{
    EventLoop loop;
    loop.post([] { std::raise(SIGINT); });
    const auto start = std::chrono::steady_clock::now();

    const bool clean = runUntilStopped(loop, [] {}, 100ms);

    CHECK_FALSE(clean);
    CHECK(std::chrono::steady_clock::now() - start >= 100ms);
    CHECK(std::chrono::steady_clock::now() - start < 5s);
}

TEST_CASE("A server whose work ends stops properly without a signal", "[server]")
{
    EventLoop loop;
    // For instance an instance whose game is over
    loop.post([&] { loop.stop(); });

    CHECK(runUntilStopped(loop, [] {}));
}
