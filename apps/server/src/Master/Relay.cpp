#include "Master/Relay.hpp"

#include "Common/RelayFrame.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/Datagram.hpp"
#include "rtype/network/Session.hpp"

#include <exception>
#include <optional>

namespace rtype::server
{

namespace
{

const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("relay");
    return instance;
}

/// How often the sessions released are looked at.
constexpr std::chrono::milliseconds SweepInterval{250};

} // namespace

Relay::Relay(const rtype::network::Address &publicAddress, const std::vector<AddressBan> &bans)
    : _public(_loop, publicAddress), _loopback(_loop, rtype::network::Address::loopbackIpv4(0)),
      _sweep(_loop), _bans(bans), _rates(rtype::network::MaxDatagramsPerSecond)
{
    _public.startReceiving([this](const rtype::network::Address &from, std::span<const std::byte> datagram) {
        inbound(from, datagram);
    });
    _loopback.startReceiving([this](const rtype::network::Address &from,
                                    std::span<const std::byte> received) { outbound(from, received); });
    _sweep.startRepeating(SweepInterval, [this] { sweep(); });
    _thread = std::thread([this] { run(); });
}

Relay::~Relay()
{
    stop();
}

void Relay::run()
{
    // A handler that throws is a bug: logged, and the relay goes on with the next datagram
    while (true) {
        try {
            _loop.run();
            return;
        } catch (const std::exception &error) {
            logger().error("The relay caught: {}", error.what());
        }
    }
}

void Relay::stop()
{
    if (_stopped) {
        return;
    }
    _stopped = true;
    // After the work posted before, the frames received already included
    _loop.post([this] {
        _sweep.cancel();
        _loop.stop();
    });
    if (_thread.joinable()) {
        _thread.join();
    }
}

void Relay::drop(Drop reason)
{
    _dropped[static_cast<std::size_t>(reason)].fetch_add(1, std::memory_order_relaxed);
}

void Relay::inbound(const rtype::network::Address &from, std::span<const std::byte> datagram)
{
    const std::optional<rtype::network::DatagramHeader> header =
        rtype::network::parseDatagramHeader(datagram);
    if (!header) {
        drop(Drop::Malformed);
        return;
    }
    if (_bans.find(from)) {
        drop(Drop::Banned);
        return;
    }
    if (!_rates.allow(from, Clock::now())) {
        drop(Drop::OverRate);
        return;
    }
    const auto session = _sessions.find(header->session);
    const auto instance = session == _sessions.end() ? _instances.end() : _instances.find(session->second);
    if (instance == _instances.end()) {
        if (logger().isEnabled(rtype::log::Level::Trace)) {
            logger().trace("Datagram from {} dropped: unknown session {}", from, header->session);
        }
        drop(Drop::UnknownSession);
        return;
    }
    // A valid header fits in a frame: its size is already checked
    _loopback.send(instance->second.address, encodeRelayFrame(from, datagram));
    _inbound.fetch_add(1, std::memory_order_relaxed);
}

void Relay::outbound(const rtype::network::Address &from, std::span<const std::byte> received)
{
    const auto instance = _instanceAt.find(from);
    if (instance == _instanceAt.end()) {
        logger().trace("Datagram from {} on the loopback dropped: not an instance", from);
        drop(Drop::NotAnInstance);
        return;
    }
    const std::optional<RelayFrame> frame = decodeRelayFrame(received);
    const std::optional<rtype::network::DatagramHeader> header =
        frame ? rtype::network::parseDatagramHeader(frame->datagram) : std::nullopt;
    if (!header) {
        logger().debug("Malformed relay frame from the instance of game {} dropped ({} bytes)",
                       instance->second, received.size());
        drop(Drop::MalformedFrame);
        return;
    }
    const auto session = _sessions.find(header->session);
    if (session == _sessions.end() || session->second != instance->second) {
        logger().debug("The instance of game {} sent for session {}, not one of its own: dropped",
                       instance->second, header->session);
        drop(Drop::WrongInstance);
        return;
    }
    _public.send(frame->client, frame->datagram);
    _outbound.fetch_add(1, std::memory_order_relaxed);
}

void Relay::sweep()
{
    const Clock::time_point now = Clock::now();
    while (!_released.empty() && _released.front().until <= now) {
        const Released &released = _released.front();
        // Unless its game exited since, and the master gave its id to a player of another game
        if (const auto found = _sessions.find(released.sessionId);
            found != _sessions.end() && found->second == released.gameId) {
            _sessions.erase(found);
        }
        _released.pop_front();
    }
}

void Relay::addInstance(std::uint32_t gameId, std::uint16_t port)
{
    _loop.post([this, gameId, port] {
        const rtype::network::Address address = rtype::network::Address::loopbackIpv4(port);
        if (const auto previous = _instances.find(gameId); previous != _instances.end()) {
            _instanceAt.erase(previous->second.address);
        }
        _instances.insert_or_assign(gameId, Instance{address});
        _instanceAt.insert_or_assign(address, gameId);
        logger().debug("Game {} behind the relay, on {}", gameId, address);
    });
}

void Relay::removeInstance(std::uint32_t gameId)
{
    _loop.post([this, gameId] {
        const auto found = _instances.find(gameId);
        if (found != _instances.end()) {
            _instanceAt.erase(found->second.address);
            _instances.erase(found);
        }
        const std::size_t sessions =
            std::erase_if(_sessions, [gameId](const auto &entry) { return entry.second == gameId; });
        logger().debug("Game {} no longer behind the relay: {} sessions forgotten", gameId, sessions);
    });
}

void Relay::addSession(std::uint32_t sessionId, std::uint32_t gameId)
{
    _loop.post([this, sessionId, gameId] { _sessions.insert_or_assign(sessionId, gameId); });
}

void Relay::releaseSession(std::uint32_t sessionId)
{
    _loop.post([this, sessionId] {
        const auto found = _sessions.find(sessionId);
        if (found != _sessions.end()) {
            _released.push_back({Clock::now() + SessionLinger, sessionId, found->second});
        }
    });
}

void Relay::setBans(std::vector<AddressBan> bans)
{
    _loop.post([this, bans = std::move(bans)] { _bans = AddressBans(bans); });
}

Relay::Counters Relay::counters() const
{
    Counters counters;
    counters.inbound = _inbound.load(std::memory_order_relaxed);
    counters.outbound = _outbound.load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < DropCount; ++i) {
        counters.dropped[i] = _dropped[i].load(std::memory_order_relaxed);
    }
    return counters;
}

} // namespace rtype::server
