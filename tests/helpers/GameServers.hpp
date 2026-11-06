#pragma once

// What the tests that run real games share: free UDP ports, the settings of instances of r-type_server, which
// run behind a relay of the test (Master/Relay.hpp), and stopping a process at once, as a crash would.

#include "Master/Games.hpp"
#include "Master/Relay.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/UdpSocket.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <random>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <signal.h>
#endif

namespace testing
{

/// `count` consecutive UDP ports free now.
inline std::uint16_t freePorts(std::uint16_t count)
{
    rtype::network::EventLoop loop;
    std::mt19937 random(std::random_device{}());
    std::uniform_int_distribution<int> base(20000, 60000);
    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto first = static_cast<std::uint16_t>(base(random));
        try {
            std::vector<rtype::network::UdpSocket> sockets;
            for (std::uint16_t i = 0; i < count; ++i) {
                sockets.emplace_back(loop,
                                     rtype::network::Address::anyIpv4(static_cast<std::uint16_t>(first + i)));
            }
            return first;
        } catch (const std::system_error &) {
            continue;
        }
    }
    FAIL("No free UDP ports");
    return 0;
}

/// Games run by real instances of r-type_server, `maxGames` at most, behind a relay of the test.
inline rtype::server::GamesSettings instances(std::uint32_t maxGames = 4)
{
    return {
        .executable = RTYPE_TEST_SERVER,
        .leadingArguments = {},
        .trailingArguments = {"--log-level", "warning"},
        .maxGames = maxGames,
    };
}

/// Stops a process at once, as a crash would.
inline void killProcess(int pid)
{
#ifdef _WIN32
    HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    REQUIRE(process != nullptr);
    TerminateProcess(process, 9);
    CloseHandle(process);
#else
    REQUIRE(::kill(pid, SIGKILL) == 0);
#endif
}

} // namespace testing
