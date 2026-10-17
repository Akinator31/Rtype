#include "rtype/network/Session.hpp"

#include "NetworkLogger.hpp"
#include "SessionPeer.hpp"

#include <array>
#include <format>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace rtype::network
{

using detail::logger;
using detail::SessionPeer;

namespace
{

/// How often the rate counters of addresses that stopped sending are forgotten.
constexpr std::chrono::seconds RatePruneInterval{10};

std::string_view describe(const SessionEnd &end)
{
    switch (end.cause) {
    case SessionEnd::Cause::Disconnected:
        switch (end.reason) {
        case DisconnectReason::GameOver:
            return "disconnected: game over";
        case DisconnectReason::Shutdown:
            return "disconnected: shutdown";
        case DisconnectReason::ProtocolError:
            return "disconnected: protocol error";
        case DisconnectReason::Quit:
            break;
        }
        return "disconnected: quit";
    case SessionEnd::Cause::TimedOut:
        return "timed out";
    case SessionEnd::Cause::ConnectTimedOut:
        return "never connected";
    case SessionEnd::Cause::Failed:
        return "the peer stopped acknowledging";
    }
    return "closed";
}

} // namespace

// --- SessionPeer ---

namespace detail
{

SessionPeer::SessionPeer(std::uint32_t sessionId, const DatagramKey &own, const DatagramKey &peer,
                         Clock::time_point now)
    : id(sessionId), ownKey(own), peerKey(peer), created(now), lastReceived(now), lastSent(now)
{
}

std::optional<std::vector<ReceivedMessage>>
SessionPeer::receive(const DatagramHeader &header, std::span<const std::byte> datagram, Clock::time_point now)
{
    // Checked before decrypting: a replayed datagram costs no decryption
    if (!channel.accepts(header.packetNumber)) {
        return std::nullopt;
    }
    const std::optional<std::vector<std::byte>> body = openDatagram(peerKey, datagram);
    if (!body) {
        return std::nullopt;
    }
    auto messages = channel.receive(header.packetNumber, *body, now);
    if (messages) {
        lastReceived = now;
    }
    return messages;
}

std::vector<std::vector<std::byte>> SessionPeer::flush(Clock::time_point now)
{
    std::vector<std::vector<std::byte>> datagrams;
    if (!channel.hasMessagesToSend(now) && now - lastSent < HeartbeatInterval) {
        return datagrams;
    }
    do {
        const std::optional<OutgoingDatagram> datagram = channel.next(now);
        if (!datagram) {
            break;
        }
        datagrams.push_back(sealDatagram(ownKey, {id, datagram->packetNumber}, datagram->body));
    } while (channel.hasMessagesToSend(now));
    if (!datagrams.empty()) {
        lastSent = now;
    }
    return datagrams;
}

std::vector<std::vector<std::byte>> SessionPeer::farewell(DisconnectReason reason)
{
    const std::array<std::byte, 1> payload{static_cast<std::byte>(reason)};
    std::vector<std::vector<std::byte>> datagrams;
    for (int i = 0; i < DisconnectRepeats; ++i) {
        const std::optional<OutgoingDatagram> datagram =
            channel.farewell(session_message::Disconnect, payload);
        if (!datagram) {
            break;
        }
        datagrams.push_back(sealDatagram(ownKey, {id, datagram->packetNumber}, datagram->body));
    }
    return datagrams;
}

DisconnectReason disconnectReasonOf(std::span<const std::byte> payload)
{
    if (payload.size() != 1) {
        return DisconnectReason::Quit;
    }
    const auto value = std::to_integer<std::uint8_t>(payload[0]);
    if (value > static_cast<std::uint8_t>(DisconnectReason::ProtocolError)) {
        return DisconnectReason::Quit;
    }
    return static_cast<DisconnectReason>(value);
}

} // namespace detail

// --- ClientSession ---

ClientSession::ClientSession(std::uint32_t sessionId, const DatagramKey &clientKey,
                             const DatagramKey &serverKey, SendFunction send)
    : _peer(std::make_unique<SessionPeer>(sessionId, clientKey, serverKey, Clock::time_point{})),
      _send(std::move(send))
{
}

ClientSession::~ClientSession() = default;
ClientSession::ClientSession(ClientSession &&) noexcept = default;
ClientSession &ClientSession::operator=(ClientSession &&) noexcept = default;

void ClientSession::start(AcceptHandler onAccepted, MessageHandler onMessage, CloseHandler onClose,
                          Clock::time_point now)
{
    if (_state != State::Idle) {
        return;
    }
    _onAccepted = std::move(onAccepted);
    _onMessage = std::move(onMessage);
    _onClose = std::move(onClose);
    _state = State::Connecting;
    _started = now;
    _peer->lastReceived = now;
    _peer->channel.send(session_message::Connect, {}, Delivery::Reliable);
    logger().debug("Session {} connecting", _peer->id);
    sendDatagrams(now);
}

void ClientSession::receive(std::span<const std::byte> datagram, Clock::time_point now)
{
    if (_state != State::Connecting && _state != State::Connected) {
        return;
    }
    const std::optional<DatagramHeader> header = parseDatagramHeader(datagram);
    if (!header || header->session != _peer->id) {
        return;
    }
    const auto messages = _peer->receive(*header, datagram, now);
    if (!messages) {
        return;
    }
    for (const ReceivedMessage &message : *messages) {
        // A handler may have closed the session
        if (_state == State::Closed) {
            return;
        }
        switch (message.type) {
        case session_message::Disconnect:
            end({SessionEnd::Cause::Disconnected, detail::disconnectReasonOf(message.payload)}, std::nullopt);
            return;
        case session_message::Accepted:
            if (_state == State::Connecting) {
                _state = State::Connected;
                logger().debug("Session {} accepted", _peer->id);
                if (_onAccepted) {
                    _onAccepted(message.payload);
                }
            }
            break;
        case session_message::Connect:
            break;
        default:
            // Before ACCEPTED, an unreliable message may overtake it: dropped
            if (_state == State::Connected && _onMessage) {
                _onMessage(message.type, message.payload);
            }
            break;
        }
    }
}

void ClientSession::update(Clock::time_point now)
{
    if (_state != State::Connecting && _state != State::Connected) {
        return;
    }
    if (_state == State::Connecting && now - _started >= ConnectTimeout) {
        end({SessionEnd::Cause::ConnectTimedOut}, std::nullopt);
        return;
    }
    if (_state == State::Connected && now - _peer->lastReceived >= SessionTimeout) {
        end({SessionEnd::Cause::TimedOut}, std::nullopt);
        return;
    }
    sendDatagrams(now);
    if (_peer->channel.failed()) {
        end({SessionEnd::Cause::Failed}, DisconnectReason::ProtocolError);
    }
}

bool ClientSession::send(std::uint8_t type, std::span<const std::byte> payload, Delivery delivery)
{
    if (_state != State::Connected) {
        return false;
    }
    return _peer->channel.send(type, payload, delivery);
}

void ClientSession::close(DisconnectReason reason)
{
    if (_state == State::Connecting || _state == State::Connected) {
        for (const auto &datagram : _peer->farewell(reason)) {
            _send(datagram);
        }
        logger().debug("Session {} closed", _peer->id);
    }
    _state = State::Closed;
    _onAccepted = nullptr;
    _onMessage = nullptr;
    _onClose = nullptr;
}

std::uint32_t ClientSession::sessionId() const
{
    return _peer->id;
}

std::optional<ClientSession::Clock::duration> ClientSession::rtt() const
{
    return _peer->channel.rtt();
}

void ClientSession::sendDatagrams(Clock::time_point now)
{
    for (const auto &datagram : _peer->flush(now)) {
        _send(datagram);
    }
}

void ClientSession::end(SessionEnd end, std::optional<DisconnectReason> farewell)
{
    if (farewell) {
        for (const auto &datagram : _peer->farewell(*farewell)) {
            _send(datagram);
        }
    }
    logger().debug("Session {} ended: {}", _peer->id, describe(end));
    _state = State::Closed;
    const CloseHandler handler = std::move(_onClose);
    _onAccepted = nullptr;
    _onMessage = nullptr;
    _onClose = nullptr;
    if (handler) {
        handler(end);
    }
}

// --- SessionHost ---

SessionHost::SessionHost(SendFunction send, Handlers handlers)
    : _send(std::move(send)), _handlers(std::move(handlers))
{
}

SessionHost::~SessionHost() = default;

void SessionHost::add(std::uint32_t session, const DatagramKey &clientKey, const DatagramKey &serverKey,
                      Clock::time_point now)
{
    if (session == 0) {
        throw std::invalid_argument("a session id is never 0");
    }
    if (_sessions.contains(session)) {
        throw std::invalid_argument(std::format("the session {} already exists", session));
    }
    // The game sends with the server key, and the client with the client key
    _sessions.emplace(session, std::make_unique<SessionPeer>(session, serverKey, clientKey, now));
    logger().debug("Session {} expects its client", session);
}

void SessionHost::receive(const Address &from, std::span<const std::byte> datagram, Clock::time_point now)
{
    const std::optional<DatagramHeader> header = parseDatagramHeader(datagram);
    if (!header) {
        return;
    }
    const auto found = _sessions.find(header->session);
    if (found == _sessions.end()) {
        if (logger().isEnabled(rtype::log::Level::Trace)) {
            logger().trace("Datagram from {} dropped: unknown session {}", from, header->session);
        }
        return;
    }
    if (!withinRate(from, now)) {
        return;
    }
    SessionPeer &peer = *found->second;
    const bool newest = header->packetNumber > peer.channel.highestReceived();
    const auto messages = peer.receive(*header, datagram, now);
    if (!messages) {
        return;
    }
    // The client's address: learned from its first authentic datagram, and followed if a home router changes
    // it
    if (!peer.address) {
        peer.address = from;
    } else if (*peer.address != from && newest) {
        logger().debug("Session {} moved from {} to {}", peer.id, *peer.address, from);
        peer.address = from;
    }

    const std::uint32_t id = header->session;
    for (const ReceivedMessage &message : *messages) {
        // A handler may have closed the session
        const auto current = _sessions.find(id);
        if (current == _sessions.end()) {
            return;
        }
        SessionPeer &session = *current->second;
        switch (message.type) {
        case session_message::Connect:
            if (!session.connected) {
                session.connected = true;
                logger().debug("Session {} connected from {}", id, from);
                if (_handlers.onConnected) {
                    _handlers.onConnected(id, from);
                }
            }
            break;
        case session_message::Disconnect:
            end(id, {SessionEnd::Cause::Disconnected, detail::disconnectReasonOf(message.payload)},
                std::nullopt);
            return;
        case session_message::Accepted:
            break;
        default:
            if (session.connected && _handlers.onMessage) {
                _handlers.onMessage(id, message.type, message.payload);
            }
            break;
        }
    }
}

void SessionHost::update(Clock::time_point now)
{
    std::vector<std::uint32_t> ids;
    ids.reserve(_sessions.size());
    for (const auto &[id, peer] : _sessions) {
        ids.push_back(id);
    }
    for (const std::uint32_t id : ids) {
        const auto found = _sessions.find(id);
        if (found == _sessions.end()) {
            continue;
        }
        SessionPeer &peer = *found->second;
        if (!peer.connected) {
            if (now - peer.created >= PendingTimeout) {
                end(id, {SessionEnd::Cause::ConnectTimedOut}, std::nullopt);
            }
            continue;
        }
        if (now - peer.lastReceived >= SessionTimeout) {
            end(id, {SessionEnd::Cause::TimedOut}, std::nullopt);
            continue;
        }
        sendDatagrams(peer, now);
        if (peer.channel.failed()) {
            end(id, {SessionEnd::Cause::Failed}, DisconnectReason::ProtocolError);
        }
    }

    if (now - _lastRatePrune >= RatePruneInterval) {
        _lastRatePrune = now;
        std::erase_if(_rates,
                      [&](const auto &entry) { return now - entry.second.windowStart >= RatePruneInterval; });
    }
}

bool SessionHost::send(std::uint32_t session, std::uint8_t type, std::span<const std::byte> payload,
                       Delivery delivery)
{
    const auto found = _sessions.find(session);
    if (found == _sessions.end() || !found->second->connected) {
        return false;
    }
    return found->second->channel.send(type, payload, delivery);
}

void SessionHost::close(std::uint32_t session, DisconnectReason reason)
{
    const auto found = _sessions.find(session);
    if (found == _sessions.end()) {
        return;
    }
    SessionPeer &peer = *found->second;
    if (peer.connected && peer.address) {
        for (const auto &datagram : peer.farewell(reason)) {
            _send(*peer.address, datagram);
        }
    }
    logger().debug("Session {} closed", session);
    _sessions.erase(found);
}

void SessionHost::closeAll(DisconnectReason reason)
{
    std::vector<std::uint32_t> ids;
    for (const auto &[id, peer] : _sessions) {
        ids.push_back(id);
    }
    for (const std::uint32_t id : ids) {
        close(id, reason);
    }
}

bool SessionHost::connected(std::uint32_t session) const
{
    const auto found = _sessions.find(session);
    return found != _sessions.end() && found->second->connected;
}

std::optional<Address> SessionHost::address(std::uint32_t session) const
{
    const auto found = _sessions.find(session);
    if (found == _sessions.end() || !found->second->connected) {
        return std::nullopt;
    }
    return found->second->address;
}

std::optional<SessionHost::Clock::duration> SessionHost::rtt(std::uint32_t session) const
{
    const auto found = _sessions.find(session);
    if (found == _sessions.end()) {
        return std::nullopt;
    }
    return found->second->channel.rtt();
}

bool SessionHost::withinRate(const Address &from, Clock::time_point now)
{
    RateCounter &counter = _rates[from];
    if (counter.count == 0 || now - counter.windowStart >= std::chrono::seconds(1)) {
        counter.windowStart = now;
        counter.count = 0;
    }
    ++counter.count;
    if (counter.count == MaxDatagramsPerSecond + 1) {
        logger().debug("{} sends more than {} datagrams per second: the others are dropped", from,
                       MaxDatagramsPerSecond);
    }
    return counter.count <= MaxDatagramsPerSecond;
}

void SessionHost::sendDatagrams(SessionPeer &peer, Clock::time_point now)
{
    if (!peer.address) {
        return;
    }
    for (const auto &datagram : peer.flush(now)) {
        _send(*peer.address, datagram);
    }
}

void SessionHost::end(std::uint32_t session, SessionEnd end, std::optional<DisconnectReason> farewell)
{
    const auto found = _sessions.find(session);
    if (found == _sessions.end()) {
        return;
    }
    SessionPeer &peer = *found->second;
    if (farewell && peer.connected && peer.address) {
        for (const auto &datagram : peer.farewell(*farewell)) {
            _send(*peer.address, datagram);
        }
    }
    logger().debug("Session {} ended: {}", session, describe(end));
    _sessions.erase(found);
    if (_handlers.onClosed) {
        _handlers.onClosed(session, end);
    }
}

} // namespace rtype::network
