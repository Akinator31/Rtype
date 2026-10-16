#pragma once

#include "rtype/network/Address.hpp"
#include "rtype/network/TcpConnection.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>

namespace rtype::network
{

/// A TCP connection that exchanges frames instead of bytes: each message arrives whole, with its type, in the
/// order it was sent (docs/protocol/lobby.md, section 3).
///
/// @code
/// rtype::network::FramedConnection server(std::move(connection));
/// server.start(
///     [](std::uint8_t type, std::span<const std::byte> payload) { /* one whole message */ },
///     [](std::error_code reason) { /* closed by the peer, lost, or invalid frame */ });
/// server.send(0x01, hello.bytes());
/// @endcode
///
/// When the peer sends an invalid frame (size 0, or above the maximum), the connection is closed, and the
/// close handler receives NetworkError::EmptyFrame or NetworkError::FrameTooLarge.
///
/// Every function can be called from any thread. The handlers are called on the loop's thread.
class FramedConnection
{
public:
    /// Called with each whole frame received, on the loop's thread. The payload is only valid during the
    /// call.
    using FrameHandler = std::function<void(std::uint8_t type, std::span<const std::byte> payload)>;
    /// Called once when the connection ends without close() being called: closed by the peer (no error),
    /// lost (the system error), or closed because the peer sent an invalid frame (a NetworkError).
    using CloseHandler = TcpConnection::CloseHandler;

private:
    struct State;
    /// Holds the TCP connection: shared with its handlers, which outlive a FramedConnection moved or
    /// destroyed
    std::shared_ptr<State> _state;
    Address _localAddress;
    Address _remoteAddress;

public:
    /// Takes a connection that has not been started yet.
    explicit FramedConnection(TcpConnection connection);
    /// Closes the connection, as close() does.
    ~FramedConnection();

    FramedConnection(const FramedConnection &) = delete;
    FramedConnection &operator=(const FramedConnection &) = delete;
    FramedConnection(FramedConnection &&) noexcept;
    FramedConnection &operator=(FramedConnection &&) noexcept;

    /// Address of this end of the connection.
    [[nodiscard]] const Address &localAddress() const { return _localAddress; }
    /// Address of the peer.
    [[nodiscard]] const Address &remoteAddress() const { return _remoteAddress; }

    /// Starts receiving: the frame handler is called for each whole frame, and the close handler when the
    /// connection ends. Call it once.
    void start(FrameHandler onFrame, CloseHandler onClose);

    /// Sends a frame, after those of the previous calls.
    /// @throws std::length_error if the payload is larger than MaxFramePayloadSize.
    void send(std::uint8_t type, std::span<const std::byte> payload);

    /// Closes the connection once the frames already given to send() are sent. The handlers are no longer
    /// called, not even for the frames already received but not handled yet.
    void close();
};

} // namespace rtype::network
