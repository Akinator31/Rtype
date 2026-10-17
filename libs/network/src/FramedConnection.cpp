#include "rtype/network/FramedConnection.hpp"

#include "NetworkLogger.hpp"
#include "rtype/network/Frame.hpp"

#include <atomic>
#include <variant>

namespace rtype::network
{

using detail::logger;

// Shared by the FramedConnection and, through weak references, by the handlers of its TCP connection: a
// handler running while the FramedConnection is destroyed keeps it alive until it returns.
struct FramedConnection::State {
    /// Both have the same functions: start(), send(), close() and the addresses
    std::variant<TcpConnection, TlsConnection> connection;
    FrameDecoder decoder;
    FrameHandler onFrame;
    CloseHandler onClose;
    /// close() was called, or the connection ended: no handler is called any more. Set from any thread.
    std::atomic<bool> closed = false;

    template <typename Connection>
    explicit State(Connection stream) : connection(std::move(stream))
    {
    }

    [[nodiscard]] const Address &remoteAddress() const
    {
        return std::visit([](const auto &stream) -> const Address & { return stream.remoteAddress(); },
                          connection);
    }

    [[nodiscard]] const Address &localAddress() const
    {
        return std::visit([](const auto &stream) -> const Address & { return stream.localAddress(); },
                          connection);
    }

    void closeConnection()
    {
        std::visit([](auto &stream) { stream.close(); }, connection);
    }

    // On the loop's thread
    void receive(std::span<const std::byte> bytes)
    {
        decoder.feed(bytes);
        while (!closed) {
            const std::optional<Frame> frame = decoder.next();
            if (!frame) {
                break;
            }
            onFrame(frame->type, frame->payload);
        }
        if (!closed && decoder.failed()) {
            logger().warning("TCP {} sent an invalid frame ({}): closing the connection", remoteAddress(),
                             decoder.error().message());
            closeConnection();
            end(decoder.error());
        }
    }

    // On the loop's thread
    void end(std::error_code reason)
    {
        if (closed.exchange(true)) {
            return;
        }
        onFrame = nullptr;
        const CloseHandler handler = std::move(onClose);
        onClose = nullptr;
        if (handler) {
            handler(reason);
        }
    }
};

FramedConnection::FramedConnection(TcpConnection connection)
    : _state(std::make_shared<State>(std::move(connection))), _localAddress(_state->localAddress()),
      _remoteAddress(_state->remoteAddress())
{
}

FramedConnection::FramedConnection(TlsConnection connection)
    : _state(std::make_shared<State>(std::move(connection))), _localAddress(_state->localAddress()),
      _remoteAddress(_state->remoteAddress())
{
}

FramedConnection::~FramedConnection()
{
    close();
}

FramedConnection::FramedConnection(FramedConnection &&) noexcept = default;

FramedConnection &FramedConnection::operator=(FramedConnection &&other) noexcept
{
    if (this != &other) {
        close();
        _state = std::move(other._state);
        _localAddress = other._localAddress;
        _remoteAddress = other._remoteAddress;
    }
    return *this;
}

void FramedConnection::start(FrameHandler onFrame, CloseHandler onClose)
{
    if (!_state) {
        return;
    }
    _state->onFrame = std::move(onFrame);
    _state->onClose = std::move(onClose);
    const std::weak_ptr<State> weak = _state;
    std::visit(
        [&weak](auto &stream) {
            stream.start(
                [weak](std::span<const std::byte> bytes) {
                    if (const auto state = weak.lock(); state && !state->closed) {
                        state->receive(bytes);
                    }
                },
                [weak](std::error_code reason) {
                    if (const auto state = weak.lock()) {
                        state->end(reason);
                    }
                });
        },
        _state->connection);
}

void FramedConnection::send(std::uint8_t type, std::span<const std::byte> payload)
{
    if (!_state) {
        return;
    }
    std::visit([frame = makeFrame(type, payload)](auto &stream) mutable { stream.send(std::move(frame)); },
               _state->connection);
}

void FramedConnection::close()
{
    if (!_state) {
        return;
    }
    _state->closed = true;
    _state->closeConnection();
}

} // namespace rtype::network
