#pragma once

#include "Master/AddressBans.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/RateLimiter.hpp"
#include "rtype/network/Timer.hpp"
#include "rtype/network/UdpSocket.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <span>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rtype::server
{

/// The one public UDP port of the master, in front of every game (docs/architecture/server.md#the-relay): it
/// reads the session in the clear header of each datagram, and forwards it to the instance of that game on
/// the loopback, in a relay frame with the address of its client (Common/RelayFrame.hpp). The instance
/// answers the same way, and the relay sends each datagram it carries to its client, from the public port.
///
/// @code
/// rtype::server::Relay relay(rtype::network::Address::anyIpv4(4242));
/// // CONFIGURE: relay.loopbackPort(); READY: the port of the instance
/// relay.addInstance(gameId, ready.udpPort);
/// relay.addSession(sessionId, gameId);   // ADD_PLAYER
/// relay.releaseSession(sessionId);       // the player left: its DISCONNECT still goes out
/// relay.removeInstance(gameId);          // its process exited
/// @endcode
///
/// **Inbound**, a datagram on the public port is dropped if its header is invalid (parseDatagramHeader()), if
/// its address is banned, if its address is over MaxDatagramsPerSecond, or if its session is unknown;
/// otherwise it goes to the instance of its session.
///
/// **Outbound**, a frame on the loopback is dropped if it comes from an address that is not an instance, if
/// it does not decode, or if the datagram it carries is not for a session of that instance: an instance only
/// ever speaks for its own players.
///
/// The relay never holds the keys of the datagrams: it routes on the clear header, the encryption stays end
/// to end, client to instance.
///
/// It runs on a thread of its own, with its own EventLoop, so that the datagrams of every game never wait on
/// the lobby (docs/architecture/threads.md). Its functions can be called from any thread: they are posted to
/// it. Its table and its copy of the bans live on its thread alone.
class Relay
{
public:
    using Clock = std::chrono::steady_clock;

    /// How long a session stays routed after its player left: the time for the `DISCONNECT` of its instance,
    /// sent three times, to go out.
    static constexpr std::chrono::seconds SessionLinger{2};

    /// Why a datagram was dropped: counted, for the tests and the measures.
    enum class Drop : std::uint8_t {
        /// Inbound: wrong size, magic, version, session 0 (parseDatagramHeader()).
        Malformed,
        /// Inbound: its address is banned.
        Banned,
        /// Inbound: its address sends more than MaxDatagramsPerSecond.
        OverRate,
        /// Inbound: no game has its session.
        UnknownSession,
        /// Outbound: a datagram on the loopback from an address that is not an instance.
        NotAnInstance,
        /// Outbound: a frame that does not decode, or whose datagram has no valid header.
        MalformedFrame,
        /// Outbound: the session of the datagram is not one of its instance.
        WrongInstance,
    };
    static constexpr std::size_t DropCount = 7;

    /// What the relay did since it started.
    struct Counters {
        /// Datagrams forwarded from a client to its instance.
        std::uint64_t inbound = 0;
        /// Datagrams forwarded from an instance to its client.
        std::uint64_t outbound = 0;
        /// Datagrams dropped, for each reason of Drop.
        std::array<std::uint64_t, DropCount> dropped{};

        [[nodiscard]] std::uint64_t droppedFor(Drop reason) const
        {
            return dropped[static_cast<std::size_t>(reason)];
        }
    };

private:
    struct Instance {
        rtype::network::Address address;
    };
    struct Released {
        Clock::time_point until;
        std::uint32_t sessionId = 0;
        std::uint32_t gameId = 0;
    };

    // Declared first, destroyed last: the sockets and the timer are destroyed before it
    rtype::network::EventLoop _loop;
    rtype::network::UdpSocket _public;
    rtype::network::UdpSocket _loopback;
    rtype::network::Timer _sweep;

    // On the thread of the relay only
    std::unordered_map<std::uint32_t, Instance> _instances;
    std::unordered_map<rtype::network::Address, std::uint32_t> _instanceAt;
    /// session → game.
    std::unordered_map<std::uint32_t, std::uint32_t> _sessions;
    /// The sessions released, with when they stop being routed, the earliest first.
    std::deque<Released> _released;
    AddressBans _bans;
    rtype::network::RateLimiter _rates;

    std::atomic<std::uint64_t> _inbound = 0;
    std::atomic<std::uint64_t> _outbound = 0;
    std::array<std::atomic<std::uint64_t>, DropCount> _dropped{};

    std::thread _thread;
    bool _stopped = false;

    void run();
    void inbound(const rtype::network::Address &from, std::span<const std::byte> datagram);
    void outbound(const rtype::network::Address &from, std::span<const std::byte> received);
    void drop(Drop reason);
    void sweep();

public:
    /// Opens the public socket on `publicAddress` and a socket for the instances on 127.0.0.1, on a port the
    /// system chooses, then starts the thread.
    /// @throws std::system_error if a socket cannot be opened: the public port is taken, for instance.
    explicit Relay(const rtype::network::Address &publicAddress, const std::vector<AddressBan> &bans = {});
    /// Stops the thread, as stop() does.
    ~Relay();

    Relay(const Relay &) = delete;
    Relay &operator=(const Relay &) = delete;
    Relay(Relay &&) = delete;
    Relay &operator=(Relay &&) = delete;

    /// The public address, with the port the system chose if 0 was given: the port of GAME_JOINED.
    [[nodiscard]] const rtype::network::Address &publicAddress() const { return _public.localAddress(); }
    /// Its port on 127.0.0.1, where the instances send their frames: the relay port of CONFIGURE.
    [[nodiscard]] std::uint16_t loopbackPort() const { return _loopback.localAddress().port(); }

    /// The instance of a game is ready: it receives on `127.0.0.1:port` (READY).
    void addInstance(std::uint32_t gameId, std::uint16_t port);
    /// The instance of a game exited: it and every session of its game are forgotten.
    void removeInstance(std::uint32_t gameId);
    /// A player joined a game (ADD_PLAYER): its datagrams go to the instance of that game.
    void addSession(std::uint32_t sessionId, std::uint32_t gameId);
    /// A player left its game: its session is forgotten after SessionLinger.
    void releaseSession(std::uint32_t sessionId);
    /// The bans of addresses changed: the relay keeps a copy, checked at each datagram.
    void setBans(std::vector<AddressBan> bans);

    /// Calls a function on the thread of the relay, after the changes given before: in the tests, to know
    /// they are applied.
    void post(std::function<void()> function) { _loop.post(std::move(function)); }

    /// What it did so far. Read from any thread.
    [[nodiscard]] Counters counters() const;

    /// Stops the thread, after the work given before, and waits for it. Nothing is forwarded after. Call it
    /// from the thread that created it, once.
    void stop();
};

} // namespace rtype::server
