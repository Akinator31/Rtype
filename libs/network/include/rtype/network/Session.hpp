#pragma once

#include "rtype/network/Address.hpp"
#include "rtype/network/Datagram.hpp"
#include "rtype/network/MessageChannel.hpp"
#include "rtype/network/RateLimiter.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>

/// The sessions of the game over UDP (docs/protocol/game.md, sections 3, 7 and 8): a client connects to its
/// game, both sides exchange messages, and the session ends on a disconnection or a timeout. Built on the
/// encrypted datagrams and the MessageChannel.
///
/// Like them, the classes here have no socket and no clock: they are given the datagrams received and the
/// time, and send through a function. So they can be tested with a fake network, and used from a game loop as
/// well as from a network thread. Each object must be used from one thread at a time.
namespace rtype::network
{

/// The session messages handled here (docs/protocol/game.md, section 8). The others are given to the
/// application.
namespace session_message
{
/// Client → game: first message of a session.
inline constexpr std::uint8_t Connect = 0x01;
/// Game → client: answer to `CONNECT`. Its payload is made by the application (the game), and given to the
/// client's application.
inline constexpr std::uint8_t Accepted = 0x02;
/// Both ways: the session ends. Its payload is the reason.
inline constexpr std::uint8_t Disconnect = 0x03;
} // namespace session_message

/// The reason in a `DISCONNECT`.
enum class DisconnectReason : std::uint8_t {
    /// The player left. Also any unknown reason received.
    Quit = 0,
    /// The game ended.
    GameOver = 1,
    /// The game process stops.
    Shutdown = 2,
    /// The peer broke the protocol, for instance by not acknowledging any more.
    ProtocolError = 3,
};

/// Why a session ended, given to the close handler. Not called when the application closes the session
/// itself.
struct SessionEnd {
    enum class Cause {
        /// The peer sent `DISCONNECT`, with `reason`.
        Disconnected,
        /// Nothing valid received for SessionTimeout: the peer crashed or lost the network.
        TimedOut,
        /// The session never connected: no `ACCEPTED` for the client within ConnectTimeout (UDP is probably
        /// blocked), no `CONNECT` for the game within PendingTimeout.
        ConnectTimedOut,
        /// The peer stopped acknowledging the reliable messages: `DISCONNECT (PROTOCOL_ERROR)` was sent.
        Failed,
    };

    Cause cause = Cause::Disconnected;
    /// The reason the peer gave, for Cause::Disconnected.
    DisconnectReason reason = DisconnectReason::Quit;

    friend bool operator==(const SessionEnd &, const SessionEnd &) = default;
};

/// Each side sends at least one datagram this often: a heartbeat when it has nothing else to send.
inline constexpr std::chrono::seconds HeartbeatInterval{1};
/// A session that received nothing valid for this long is closed.
inline constexpr std::chrono::seconds SessionTimeout{5};
/// A client without `ACCEPTED` for this long gives up.
inline constexpr std::chrono::seconds ConnectTimeout{5};
/// A game forgets a session without `CONNECT` for this long (docs/protocol/instances.md, `ADD_PLAYER`).
inline constexpr std::chrono::seconds PendingTimeout{10};
/// `DISCONNECT` is sent in this many datagrams in a row, since it is not reliable.
inline constexpr int DisconnectRepeats = 3;
/// A game drops the datagrams of a source address beyond this many in a second, before decrypting them.
inline constexpr std::size_t MaxDatagramsPerSecond = 200;

namespace detail
{
class SessionPeer;
} // namespace detail

/// The client side of a session with a game: the session id and the keys come from the master
/// (`GAME_JOINED`).
///
/// @code
/// rtype::network::UdpSocket socket(loop, rtype::network::Address::anyIpv4(0));
/// const rtype::network::Address game = joined.gameAddress;
/// rtype::network::ClientSession session(joined.sessionId, joined.clientKey, joined.serverKey,
///     [&](std::span<const std::byte> datagram) { socket.send(game, datagram); });
///
/// socket.startReceiving([&](const rtype::network::Address &from, std::span<const std::byte> datagram) {
///     if (from == game) {
///         session.receive(datagram, Clock::now());
///     }
/// });
/// session.start(
///     [](std::span<const std::byte> accepted) { /* ACCEPTED: slot, tick rate, tick */ },
///     [](std::uint8_t type, std::span<const std::byte> payload) { /* a game message */ },
///     [](rtype::network::SessionEnd end) { /* closed by the game, timed out... */ },
///     Clock::now());
///
/// // Every frame, after loop.poll():
/// session.send(Input::Type, input, rtype::network::Delivery::Unreliable);
/// session.update(Clock::now()); // sends, heartbeats, timeouts
/// @endcode
///
/// It ignores the source address: give it only the datagrams of the game's address.
class ClientSession
{
public:
    using Clock = MessageChannel::Clock;
    /// Sends a datagram to the game.
    using SendFunction = std::function<void(std::span<const std::byte> datagram)>;
    /// Called once, with the payload of `ACCEPTED`. The data is only valid during the call.
    using AcceptHandler = std::function<void(std::span<const std::byte> payload)>;
    /// Called for each message received once accepted. The data is only valid during the call.
    using MessageHandler = std::function<void(std::uint8_t type, std::span<const std::byte> payload)>;
    /// Called once if the session ends without close() being called.
    using CloseHandler = std::function<void(SessionEnd end)>;

    enum class State {
        /// Created, start() not called yet.
        Idle,
        /// `CONNECT` sent, waiting for `ACCEPTED`.
        Connecting,
        /// `ACCEPTED` received: game messages can be sent.
        Connected,
        Closed,
    };

private:
    std::unique_ptr<detail::SessionPeer> _peer;
    SendFunction _send;
    AcceptHandler _onAccepted;
    MessageHandler _onMessage;
    CloseHandler _onClose;
    State _state = State::Idle;
    Clock::time_point _started;

    void sendDatagrams(Clock::time_point now);
    void end(SessionEnd end, std::optional<DisconnectReason> farewell);

public:
    ClientSession(std::uint32_t sessionId, const DatagramKey &clientKey, const DatagramKey &serverKey,
                  SendFunction send);
    ~ClientSession();

    ClientSession(const ClientSession &) = delete;
    ClientSession &operator=(const ClientSession &) = delete;
    ClientSession(ClientSession &&) noexcept;
    ClientSession &operator=(ClientSession &&) noexcept;

    /// Sends `CONNECT`, sent again until the game answers. Call it once.
    void start(AcceptHandler onAccepted, MessageHandler onMessage, CloseHandler onClose,
               Clock::time_point now);

    /// Handles a datagram received from the game. Datagrams that are not authentic, replayed or malformed are
    /// dropped silently.
    void receive(std::span<const std::byte> datagram, Clock::time_point now);

    /// Sends what is waiting (messages, resends, acknowledgements, a heartbeat if nothing was sent for a
    /// second), and closes the session after a timeout. Call it regularly: every frame or every tick.
    void update(Clock::time_point now);

    /// Queues a game message, sent by the next update().
    /// @return false before `ACCEPTED` (the protocol forbids it) or once closed.
    /// @throws std::length_error if the payload does not fit in a datagram.
    bool send(std::uint8_t type, std::span<const std::byte> payload, Delivery delivery);

    /// Ends the session: sends `DISCONNECT` with the reason, then forgets it. The handlers are no longer
    /// called.
    void close(DisconnectReason reason = DisconnectReason::Quit);

    [[nodiscard]] State state() const { return _state; }
    [[nodiscard]] std::uint32_t sessionId() const;
    /// The round-trip time to the game, once measured: the ping.
    [[nodiscard]] std::optional<Clock::duration> rtt() const;
};

/// The game side: the sessions of the players of one game, on one UDP socket. The master announces each one
/// (`ADD_PLAYER`) with its id and keys; the client then connects to it.
///
/// @code
/// rtype::network::UdpSocket socket(loop, rtype::network::Address::anyIpv4(port));
/// rtype::network::SessionHost host(
///     [&](const rtype::network::Address &to, std::span<const std::byte> datagram) { socket.send(to,
///     datagram); },
///     {
///         .onConnected = [&](std::uint32_t session, const rtype::network::Address &from) {
///             host.send(session, Accepted::Type, accepted(session), rtype::network::Delivery::Reliable);
///         },
///         .onMessage = [&](std::uint32_t session, std::uint8_t type, std::span<const std::byte> payload) {},
///         .onClosed = [&](std::uint32_t session, rtype::network::SessionEnd end) {},
///     });
/// socket.startReceiving([&](const rtype::network::Address &from, std::span<const std::byte> datagram) {
///     host.receive(from, datagram, Clock::now());
/// });
///
/// host.add(sessionId, clientKey, serverKey, Clock::now()); // on ADD_PLAYER
/// // Every tick, on the network thread:
/// host.update(Clock::now());
/// @endcode
///
/// The handlers may call the functions of the host, including close() of the session they are about.
class SessionHost
{
public:
    using Clock = MessageChannel::Clock;
    /// Sends a datagram to a client.
    using SendFunction = std::function<void(const Address &to, std::span<const std::byte> datagram)>;

    struct Handlers {
        /// A client connected (`CONNECT`): the application answers `ACCEPTED` with send().
        std::function<void(std::uint32_t session, const Address &from)> onConnected;
        /// A message from a connected client. The data is only valid during the call.
        std::function<void(std::uint32_t session, std::uint8_t type, std::span<const std::byte> payload)>
            onMessage;
        /// A session ended without close() being called.
        std::function<void(std::uint32_t session, SessionEnd end)> onClosed;
    };

private:
    SendFunction _send;
    Handlers _handlers;
    std::unordered_map<std::uint32_t, std::unique_ptr<detail::SessionPeer>> _sessions;
    RateLimiter _rates;

    void sendDatagrams(detail::SessionPeer &peer, Clock::time_point now);
    void end(std::uint32_t session, SessionEnd end, std::optional<DisconnectReason> farewell);

public:
    SessionHost(SendFunction send, Handlers handlers);
    ~SessionHost();

    SessionHost(const SessionHost &) = delete;
    SessionHost &operator=(const SessionHost &) = delete;

    /// Expects a client on a new session. It must connect within PendingTimeout.
    /// @throws std::invalid_argument if the id is 0 or already used.
    void add(std::uint32_t session, const DatagramKey &clientKey, const DatagramKey &serverKey,
             Clock::time_point now);

    /// Handles a datagram received on the socket, in the order of the protocol: size and header, known
    /// session, rate of its source address, replay, tag, body. Anything that fails is dropped silently.
    void receive(const Address &from, std::span<const std::byte> datagram, Clock::time_point now);

    /// Sends what is waiting for every connected session (messages, resends, acknowledgements, heartbeats),
    /// and closes the sessions that timed out. Call it regularly: every tick.
    void update(Clock::time_point now);

    /// Queues a message for a connected session, sent by the next update().
    /// @return false if the session is unknown or not connected.
    /// @throws std::length_error if the payload does not fit in a datagram.
    bool send(std::uint32_t session, std::uint8_t type, std::span<const std::byte> payload,
              Delivery delivery);

    /// Ends a session: sends `DISCONNECT` with the reason if it is connected, then forgets it. Its close
    /// handler is not called. Does nothing for an unknown session.
    void close(std::uint32_t session, DisconnectReason reason);

    /// Ends every session, as close() does: when the game stops.
    void closeAll(DisconnectReason reason);

    [[nodiscard]] bool contains(std::uint32_t session) const { return _sessions.contains(session); }
    /// The session connected: its client sent `CONNECT`.
    [[nodiscard]] bool connected(std::uint32_t session) const;
    /// Where the client of a connected session sends from.
    [[nodiscard]] std::optional<Address> address(std::uint32_t session) const;
    /// The round-trip time to the client of a session, once measured.
    [[nodiscard]] std::optional<Clock::duration> rtt(std::uint32_t session) const;
    /// Reliable messages sent to the client of a session and not acknowledged yet, 0 for an unknown session:
    /// a game that ends waits for its last messages to arrive.
    [[nodiscard]] std::size_t pendingReliable(std::uint32_t session) const;
    /// Number of sessions, connected or not.
    [[nodiscard]] std::size_t size() const { return _sessions.size(); }
};

} // namespace rtype::network
