#pragma once

// What the tests that run real games share: free UDP ports for their instances, the settings of instances of
// r-type_server, and stopping a process at once, as a crash would.

#include "Master/Games.hpp"
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

/// `count` consecutive UDP ports free now, for the games of a test.
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

/// Games run by real instances of r-type_server, on free ports.
inline rtype::server::GamesSettings instances(std::uint16_t ports = 4)
{
    const std::uint16_t first = freePorts(ports);
    return {
        .executable = RTYPE_TEST_SERVER,
        .leadingArguments = {},
        .trailingArguments = {"--log-level", "warning"},
        .firstPort = first,
        .lastPort = static_cast<std::uint16_t>(first + ports - 1),
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
