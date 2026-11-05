#pragma once

// A WebSocket client for the tests of /api/events, on the loop of the test: the upgrade request with the
// headers a test gives (its cookie, its origin), then the text frames of the server. Only what the tests
// need: no fragments, no extension, nothing sent after the upgrade.

#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/TcpConnection.hpp"

#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace testing
{

struct WebSocketClient {
    std::optional<rtype::network::TcpConnection> connection;
    /// The status of the answer to the upgrade: 101 when accepted.
    std::optional<int> status;
    /// The response, when the upgrade was refused.
    std::string refusal;
    /// The text frames received, in order.
    std::vector<std::string> messages;
    bool closed = false;

    WebSocketClient(rtype::network::EventLoop &loop, const rtype::network::Address &server,
                    const std::vector<std::string> &headers, const std::string &path = "/api/events")
    {
        std::string request =
            std::format("GET {} HTTP/1.1\r\nHost: {}\r\nUpgrade: websocket\r\n"
                        "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                        "Sec-WebSocket-Version: 13\r\n",
                        path, server);
        for (const std::string &header : headers) {
            request += header + "\r\n";
        }
        request += "\r\n";
        rtype::network::TcpConnection::connect(
            loop, server,
            [this, request](std::error_code error, std::optional<rtype::network::TcpConnection> tcp) {
                if (error || !tcp) {
                    closed = true;
                    return;
                }
                connection.emplace(std::move(*tcp));
                connection->start([this](std::span<const std::byte> bytes) { received(bytes); },
                                  [this](std::error_code) { closed = true; });
                connection->send(std::as_bytes(std::span(request)));
            });
    }

    ~WebSocketClient()
    {
        if (connection) {
            connection->close();
        }
    }

    WebSocketClient(const WebSocketClient &) = delete;
    WebSocketClient &operator=(const WebSocketClient &) = delete;

private:
    std::string _buffer;

    void received(std::span<const std::byte> bytes)
    {
        _buffer.append(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        if (!status) {
            const std::size_t end = _buffer.find("\r\n\r\n");
            if (end == std::string::npos) {
                return;
            }
            // HTTP/1.1 101 Switching Protocols
            status = std::stoi(_buffer.substr(_buffer.find(' ') + 1, 3));
            if (*status != 101) {
                refusal = _buffer;
                return;
            }
            _buffer.erase(0, end + 4);
        }
        if (*status != 101) {
            refusal += std::string(reinterpret_cast<const char *>(bytes.data()), bytes.size());
            return;
        }
        frames();
    }

    /// Reads the whole frames of the buffer: the server never masks its frames.
    void frames()
    {
        for (;;) {
            if (_buffer.size() < 2) {
                return;
            }
            const auto byte = [&](std::size_t index) { return static_cast<std::uint8_t>(_buffer[index]); };
            const std::uint8_t opcode = byte(0) & 0x0F;
            std::uint64_t length = byte(1) & 0x7F;
            std::size_t header = 2;
            if (length == 126) {
                if (_buffer.size() < 4) {
                    return;
                }
                length = (static_cast<std::uint64_t>(byte(2)) << 8) | byte(3);
                header = 4;
            } else if (length == 127) {
                if (_buffer.size() < 10) {
                    return;
                }
                length = 0;
                for (std::size_t i = 2; i < 10; ++i) {
                    length = (length << 8) | byte(i);
                }
                header = 10;
            }
            if (_buffer.size() < header + length) {
                return;
            }
            const std::string payload = _buffer.substr(header, static_cast<std::size_t>(length));
            _buffer.erase(0, header + static_cast<std::size_t>(length));
            if (opcode == 0x1) {
                messages.push_back(payload);
            } else if (opcode == 0x8) {
                closed = true;
            }
        }
    }
};

} // namespace testing
