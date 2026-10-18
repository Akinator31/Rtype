#pragma once

// What the tests of the lobby and of the games share: polling a loop until something happens, and a client
// of the lobby over TLS, pinning the key of the server.

#include "rtype/lobby/Messages.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/FramedConnection.hpp"
#include "rtype/network/TcpConnection.hpp"
#include "rtype/network/Tls.hpp"
#include "rtype/network/TlsConnection.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace testing
{

/// Long enough for any machine of the CI, short enough for a test that fails.
inline constexpr std::chrono::milliseconds Patience{10000};

/// Polls the loop until the condition holds, at most `limit`.
inline bool waitUntil(rtype::network::EventLoop &loop, const std::function<bool()> &condition,
                      std::chrono::milliseconds limit = Patience)
{
    const auto deadline = std::chrono::steady_clock::now() + limit;
    while (!condition()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        loop.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

/// Lets the loop run for a while, for what must not happen.
inline void runFor(rtype::network::EventLoop &loop, std::chrono::milliseconds duration)
{
    (void)waitUntil(loop, [] { return false; }, duration);
}

/// A client of the lobby: TCP, then TLS pinning the key of the server, then frames.
struct LobbyClient {
    using Bytes = std::vector<std::byte>;

    std::optional<rtype::network::FramedConnection> connection;
    std::vector<std::pair<std::uint8_t, Bytes>> received;
    std::optional<std::error_code> closed;
    bool secured = false;

    LobbyClient(rtype::network::EventLoop &loop, const rtype::network::Address &server,
                const rtype::network::KeyFingerprint &pinned)
    {
        rtype::network::TcpConnection::connect(
            loop, server,
            [this, pinned](std::error_code error, std::optional<rtype::network::TcpConnection> tcp) {
                if (error || !tcp) {
                    closed = error;
                    return;
                }
                connection.emplace(rtype::network::TlsConnection::client(std::move(*tcp), pinned));
                connection->start(
                    [this](std::uint8_t type, std::span<const std::byte> payload) {
                        received.emplace_back(type, Bytes(payload.begin(), payload.end()));
                    },
                    [this](std::error_code reason) { closed = reason; }, [this] { secured = true; });
            });
    }

    void send(std::uint8_t type, std::span<const std::byte> payload = {}) { connection->send(type, payload); }

    void hello(const std::string &name)
    {
        send(rtype::lobby::message_type::Hello,
             rtype::lobby::encode(rtype::lobby::HelloMessage{rtype::lobby::ProtocolVersion, name}));
    }

    /// The reply number `index`, once received.
    std::optional<std::pair<std::uint8_t, Bytes>> reply(rtype::network::EventLoop &loop,
                                                        std::size_t index = 0)
    {
        if (!waitUntil(loop, [&] { return received.size() > index; })) {
            return std::nullopt;
        }
        return received[index];
    }

    /// The error of reply number `index`, if it is one.
    std::optional<rtype::lobby::ErrorMessage> error(rtype::network::EventLoop &loop, std::size_t index = 0)
    {
        const auto answer = reply(loop, index);
        if (!answer || answer->first != rtype::lobby::message_type::Error) {
            return std::nullopt;
        }
        return rtype::lobby::decodeError(answer->second);
    }

    /// The last message of this type received, if any.
    [[nodiscard]] std::optional<Bytes> last(std::uint8_t type) const
    {
        for (auto message = received.rbegin(); message != received.rend(); ++message) {
            if (message->first == type) {
                return message->second;
            }
        }
        return std::nullopt;
    }

    /// The number of messages of this type received.
    [[nodiscard]] std::size_t count(std::uint8_t type) const
    {
        std::size_t total = 0;
        for (const auto &[received, payload] : received) {
            total += received == type ? 1 : 0;
        }
        return total;
    }
};

} // namespace testing
