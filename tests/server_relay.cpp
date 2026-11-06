// Tests of the relay of the master, alone: real sockets on the loopback, fake clients and fake instances
// (docs/architecture/server.md#the-relay). The games behind a real relay are in server_games.cpp.

#include "Common/RelayFrame.hpp"
#include "LobbyClient.hpp"
#include "Master/AddressBans.hpp"
#include "Master/Relay.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/Datagram.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/RateLimiter.hpp"
#include "rtype/network/Session.hpp"
#include "rtype/network/UdpSocket.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

using namespace rtype::server;
using namespace std::chrono_literals;
using rtype::network::Address;
using rtype::network::DatagramHeader;
using rtype::network::DatagramKey;
using rtype::network::EventLoop;
using rtype::network::UdpSocket;
using testing::runFor;
using testing::waitUntil;
using Bytes = std::vector<std::byte>;
using Drop = Relay::Drop;

namespace
{

/// A datagram of the game for a session: a valid header, a body sealed with a key of its own.
Bytes datagram(std::uint32_t session, std::uint64_t packet = 1)
{
    static const DatagramKey key = DatagramKey::generate();
    const Bytes body{std::byte{0x42}, std::byte{static_cast<unsigned char>(packet)}};
    return rtype::network::sealDatagram(key, DatagramHeader{session, packet}, body);
}

/// A socket on the loopback that keeps what it receives.
struct Peer {
    UdpSocket socket;
    std::vector<std::pair<Address, Bytes>> received;

    explicit Peer(EventLoop &loop) : socket(loop, Address::loopbackIpv4(0))
    {
        socket.startReceiving([this](const Address &from, std::span<const std::byte> data) {
            received.emplace_back(from, Bytes(data.begin(), data.end()));
        });
    }

    [[nodiscard]] const Address &address() const { return socket.localAddress(); }
    [[nodiscard]] std::uint16_t port() const { return socket.localAddress().port(); }
};

/// A relay on the loopback, and the loop of the test for its peers.
struct RelayFixture {
    EventLoop loop;
    Relay relay{Address::loopbackIpv4(0)};

    [[nodiscard]] Address publicAddress() const { return relay.publicAddress(); }
    [[nodiscard]] Address loopbackAddress() const { return Address::loopbackIpv4(relay.loopbackPort()); }

    /// Waits until the changes given to the relay so far are applied on its thread.
    void sync()
    {
        std::atomic<bool> done = false;
        relay.post([&done] { done = true; });
        REQUIRE(waitUntil(loop, [&] { return done.load(); }));
    }

    /// Waits until the relay dropped `count` datagrams for this reason.
    bool dropped(Drop reason, std::uint64_t count)
    {
        return waitUntil(loop, [&] { return relay.counters().droppedFor(reason) >= count; });
    }
};

AddressBan banOf(const Address &address, std::chrono::system_clock::duration remaining)
{
    const auto now = std::chrono::system_clock::now();
    return {.address = address,
            .createdAt = now,
            .expiresAt = now + remaining,
            .reason = "test",
            .actor = "test"};
}

} // namespace

TEST_CASE("Every address has its own count of datagrams per second", "[relay]")
{
    using Clock = rtype::network::RateLimiter::Clock;
    rtype::network::RateLimiter rates(3);
    const Address first = Address::loopbackIpv4(1000);
    const Address second = Address::loopbackIpv4(1001);
    const Clock::time_point start = Clock::now();

    CHECK(rates.allow(first, start));
    CHECK(rates.allow(first, start));
    CHECK(rates.allow(first, start + 500ms));
    CHECK_FALSE(rates.allow(first, start + 900ms));
    CHECK(rates.allow(second, start + 900ms));
    // A new second
    CHECK(rates.allow(first, start + 1s));
    // An address that stopped sending is forgotten
    CHECK(rates.tracked() == 2);
    CHECK(rates.allow(second, start + rtype::network::RateLimiter::PruneInterval + 2s));
    CHECK(rates.tracked() == 1);
}

TEST_CASE("The bans of addresses tell who follows them when they change", "[relay]")
{
    AddressBans bans;
    int changes = 0;
    bans.setChangeHandler([&] { ++changes; });
    const Address address = Address::loopbackIpv4(0);

    bans.add(banOf(address, 1h));
    CHECK(changes == 1);
    CHECK(bans.remove(Address::loopbackIpv4(1234)));
    CHECK(changes == 2);
    // Nothing lifted: nothing changed
    CHECK_FALSE(bans.remove(address));
    CHECK(changes == 2);
}

TEST_CASE("The relay forwards each datagram to the instance of its session, and the answers to its client",
          "[relay]")
{
    RelayFixture fixture;
    Peer firstInstance(fixture.loop);
    Peer secondInstance(fixture.loop);
    Peer alice(fixture.loop);
    Peer bob(fixture.loop);
    fixture.relay.addInstance(1, firstInstance.port());
    fixture.relay.addInstance(2, secondInstance.port());
    fixture.relay.addSession(10, 1);
    fixture.relay.addSession(20, 2);
    fixture.sync();

    // Inbound: in a frame with the address of its client, from the loopback port of the relay
    const Bytes fromAlice = datagram(10);
    const Bytes fromBob = datagram(20);
    alice.socket.send(fixture.publicAddress(), fromAlice);
    bob.socket.send(fixture.publicAddress(), fromBob);
    REQUIRE(waitUntil(fixture.loop,
                      [&] { return !firstInstance.received.empty() && !secondInstance.received.empty(); }));
    CHECK(firstInstance.received.size() == 1);
    CHECK(secondInstance.received.size() == 1);
    CHECK(firstInstance.received[0].first == fixture.loopbackAddress());
    const std::optional<RelayFrame> frame = decodeRelayFrame(firstInstance.received[0].second);
    REQUIRE(frame);
    CHECK(frame->client == alice.address());
    CHECK(Bytes(frame->datagram.begin(), frame->datagram.end()) == fromAlice);
    const std::optional<RelayFrame> other = decodeRelayFrame(secondInstance.received[0].second);
    REQUIRE(other);
    CHECK(other->client == bob.address());

    // Outbound: the datagram alone, from the public port
    const Bytes toAlice = datagram(10, 2);
    firstInstance.socket.send(fixture.loopbackAddress(), encodeRelayFrame(alice.address(), toAlice));
    REQUIRE(waitUntil(fixture.loop, [&] { return !alice.received.empty(); }));
    CHECK(alice.received[0].first == fixture.publicAddress());
    CHECK(alice.received[0].second == toAlice);
    CHECK(bob.received.empty());

    const Relay::Counters counters = fixture.relay.counters();
    CHECK(counters.inbound == 2);
    CHECK(counters.outbound == 1);
    CHECK(std::ranges::all_of(counters.dropped, [](std::uint64_t count) { return count == 0; }));
}

TEST_CASE("The relay drops what it cannot route", "[relay]")
{
    RelayFixture fixture;
    Peer firstInstance(fixture.loop);
    Peer secondInstance(fixture.loop);
    Peer client(fixture.loop);
    fixture.relay.addInstance(1, firstInstance.port());
    fixture.relay.addInstance(2, secondInstance.port());
    fixture.relay.addSession(10, 1);
    fixture.relay.addSession(20, 2);
    fixture.sync();

    SECTION("Inbound, a datagram that is not one of the game")
    {
        client.socket.send(fixture.publicAddress(), Bytes(40, std::byte{0x7F}));
        CHECK(fixture.dropped(Drop::Malformed, 1));
    }
    SECTION("Inbound, a session no game has")
    {
        client.socket.send(fixture.publicAddress(), datagram(30));
        CHECK(fixture.dropped(Drop::UnknownSession, 1));
    }
    SECTION("Outbound, an instance that sends for a player of another game")
    {
        firstInstance.socket.send(fixture.loopbackAddress(),
                                  encodeRelayFrame(client.address(), datagram(20)));
        CHECK(fixture.dropped(Drop::WrongInstance, 1));
    }
    SECTION("Outbound, a frame from a port of the loopback that is not an instance")
    {
        Peer stranger(fixture.loop);
        stranger.socket.send(fixture.loopbackAddress(), encodeRelayFrame(client.address(), datagram(10)));
        CHECK(fixture.dropped(Drop::NotAnInstance, 1));
    }
    SECTION("Outbound, a frame that does not decode, or whose datagram is not one of the game")
    {
        firstInstance.socket.send(fixture.loopbackAddress(), Bytes(10, std::byte{4}));
        firstInstance.socket.send(fixture.loopbackAddress(),
                                  encodeRelayFrame(client.address(), Bytes(40, std::byte{0x7F})));
        CHECK(fixture.dropped(Drop::MalformedFrame, 2));
    }
    runFor(fixture.loop, 50ms);
    CHECK(firstInstance.received.empty());
    CHECK(secondInstance.received.empty());
    CHECK(client.received.empty());
    CHECK(fixture.relay.counters().inbound == 0);
    CHECK(fixture.relay.counters().outbound == 0);
}

TEST_CASE("The relay drops the datagrams of a banned address, until its ban is lifted or expires", "[relay]")
{
    RelayFixture fixture;
    Peer instance(fixture.loop);
    Peer client(fixture.loop);
    fixture.relay.addInstance(1, instance.port());
    fixture.relay.addSession(10, 1);

    // The ban is about the IP address, whatever the port
    fixture.relay.setBans({banOf(client.address().withPort(0), 1h)});
    fixture.sync();
    client.socket.send(fixture.publicAddress(), datagram(10, 1));
    CHECK(fixture.dropped(Drop::Banned, 1));

    fixture.relay.setBans({});
    fixture.sync();
    client.socket.send(fixture.publicAddress(), datagram(10, 2));
    CHECK(waitUntil(fixture.loop, [&] { return instance.received.size() == 1; }));

    fixture.relay.setBans({banOf(client.address().withPort(0), -1s)});
    fixture.sync();
    client.socket.send(fixture.publicAddress(), datagram(10, 3));
    CHECK(waitUntil(fixture.loop, [&] { return instance.received.size() == 2; }));
    CHECK(fixture.relay.counters().droppedFor(Drop::Banned) == 1);
}

TEST_CASE("The relay drops the datagrams of an address beyond its rate", "[relay]")
{
    RelayFixture fixture;
    Peer instance(fixture.loop);
    Peer client(fixture.loop);
    fixture.relay.addInstance(1, instance.port());
    fixture.relay.addSession(10, 1);
    fixture.sync();

    constexpr std::size_t Sent = rtype::network::MaxDatagramsPerSecond + 20;
    for (std::size_t i = 1; i <= Sent; ++i) {
        client.socket.send(fixture.publicAddress(), datagram(10, i));
    }

    CHECK(fixture.dropped(Drop::OverRate, 1));
    runFor(fixture.loop, 100ms);
    CHECK(fixture.relay.counters().inbound <= rtype::network::MaxDatagramsPerSecond);
    CHECK(instance.received.size() <= rtype::network::MaxDatagramsPerSecond);
}

TEST_CASE("A session released is still routed for a while, a game gone takes its sessions", "[relay]")
{
    RelayFixture fixture;
    Peer instance(fixture.loop);
    Peer client(fixture.loop);
    fixture.relay.addInstance(1, instance.port());
    fixture.relay.addSession(10, 1);
    fixture.relay.addSession(11, 1);
    fixture.relay.releaseSession(10);
    fixture.sync();

    // Within SessionLinger, the DISCONNECT of the instance goes out
    instance.socket.send(fixture.loopbackAddress(), encodeRelayFrame(client.address(), datagram(10)));
    REQUIRE(waitUntil(fixture.loop, [&] { return client.received.size() == 1; }));

    // Then the session is forgotten, the others of its game kept
    runFor(fixture.loop, Relay::SessionLinger + 500ms);
    client.socket.send(fixture.publicAddress(), datagram(10, 2));
    CHECK(fixture.dropped(Drop::UnknownSession, 1));
    client.socket.send(fixture.publicAddress(), datagram(11, 2));
    CHECK(waitUntil(fixture.loop, [&] { return instance.received.size() == 1; }));

    // The game gone: its sessions and its address with it
    fixture.relay.removeInstance(1);
    fixture.sync();
    client.socket.send(fixture.publicAddress(), datagram(11, 3));
    CHECK(fixture.dropped(Drop::UnknownSession, 2));
    instance.socket.send(fixture.loopbackAddress(), encodeRelayFrame(client.address(), datagram(11, 4)));
    CHECK(fixture.dropped(Drop::NotAnInstance, 1));
}

TEST_CASE("A session released whose id goes to another game once its own is gone stays routed", "[relay]")
{
    RelayFixture fixture;
    Peer first(fixture.loop);
    Peer second(fixture.loop);
    Peer client(fixture.loop);
    fixture.relay.addInstance(1, first.port());
    fixture.relay.addSession(10, 1);
    fixture.relay.releaseSession(10);
    fixture.relay.removeInstance(1);
    // The master may draw the id again once the instance of its game exited
    fixture.relay.addInstance(2, second.port());
    fixture.relay.addSession(10, 2);

    runFor(fixture.loop, Relay::SessionLinger + 500ms);
    client.socket.send(fixture.publicAddress(), datagram(10));
    CHECK(waitUntil(fixture.loop, [&] { return second.received.size() == 1; }));
    CHECK(first.received.empty());
}

TEST_CASE("The relay stops after the work given before", "[relay]")
{
    EventLoop loop;
    Peer instance(loop);
    Peer client(loop);
    std::optional<Relay> relay(std::in_place, Address::loopbackIpv4(0));
    relay->addInstance(1, instance.port());
    relay->addSession(10, 1);
    std::atomic<bool> applied = false;
    relay->post([&applied] { applied = true; });

    relay->stop();
    CHECK(applied);
    relay->stop();
    relay.reset();
}

// The cost of the relay, measured, not checked: run with `rtype_tests "[.relay-latency]"`. 8 games of 4
// players at 60 Hz, each datagram echoed by its instance: the round trip through the relay, against the same
// echo with no relay.
TEST_CASE("The latency the relay adds, with 8 games of 4 players at 60 Hz", "[.relay-latency]")
{
    using Clock = std::chrono::steady_clock;
    constexpr int Games = 8;
    constexpr int PlayersPerGame = 4;
    constexpr int Rounds = 600; // 10 seconds at 60 Hz

    const auto measure = [&](bool relayed) {
        EventLoop loop;
        std::optional<Relay> relay;
        if (relayed) {
            relay.emplace(Address::loopbackIpv4(0));
        }
        std::vector<std::unique_ptr<UdpSocket>> instances;
        for (int game = 0; game < Games; ++game) {
            auto &socket =
                instances.emplace_back(std::make_unique<UdpSocket>(loop, Address::loopbackIpv4(0)));
            UdpSocket *raw = socket.get();
            // The instance answers each datagram at once, to the relay or to the client that sent it
            raw->startReceiving([raw](const Address &from, std::span<const std::byte> received) {
                raw->send(from, received);
            });
            if (relay) {
                relay->addInstance(static_cast<std::uint32_t>(game + 1), raw->localAddress().port());
            }
        }
        struct Client {
            std::unique_ptr<UdpSocket> socket;
            Address target;
            Bytes datagram;
            Clock::time_point sentAt;
        };
        std::vector<Client> clients;
        std::vector<double> latencies;
        for (int game = 0; game < Games; ++game) {
            for (int player = 0; player < PlayersPerGame; ++player) {
                const auto session = static_cast<std::uint32_t>(((game + 1) * 100) + player);
                if (relay) {
                    relay->addSession(session, static_cast<std::uint32_t>(game + 1));
                }
                Client &client = clients.emplace_back(
                    Client{std::make_unique<UdpSocket>(loop, Address::loopbackIpv4(0)),
                           relay ? relay->publicAddress()
                                 : instances[static_cast<std::size_t>(game)]->localAddress(),
                           datagram(session),
                           {}});
                const std::size_t index = clients.size() - 1;
                client.socket->startReceiving([&, index](const Address &, std::span<const std::byte>) {
                    latencies.push_back(
                        std::chrono::duration<double, std::micro>(Clock::now() - clients[index].sentAt)
                            .count());
                });
            }
        }
        runFor(loop, 50ms);
        Clock::time_point next = Clock::now();
        for (int round = 0; round < Rounds; ++round) {
            for (Client &client : clients) {
                client.sentAt = Clock::now();
                client.socket->send(client.target, client.datagram);
            }
            next += std::chrono::microseconds(16667);
            while (Clock::now() < next) {
                loop.poll();
            }
        }
        runFor(loop, 50ms);
        std::ranges::sort(latencies);
        return latencies;
    };

    const std::vector<double> direct = measure(false);
    const std::vector<double> relayed = measure(true);
    REQUIRE_FALSE(direct.empty());
    REQUIRE_FALSE(relayed.empty());
    const auto at = [](const std::vector<double> &sorted, double quantile) {
        return sorted[static_cast<std::size_t>(quantile * static_cast<double>(sorted.size() - 1))];
    };
    std::printf("%d games of %d players at 60 Hz, %zu round trips (direct), %zu (relayed)\n", Games,
                PlayersPerGame, direct.size(), relayed.size());
    std::printf("Round trip, direct:  median %.1f us, p99 %.1f us\n", at(direct, 0.5), at(direct, 0.99));
    std::printf("Round trip, relayed: median %.1f us, p99 %.1f us\n", at(relayed, 0.5), at(relayed, 0.99));
    std::printf("Added by the relay:  median %.1f us, p99 %.1f us\n", at(relayed, 0.5) - at(direct, 0.5),
                at(relayed, 0.99) - at(direct, 0.99));
}
