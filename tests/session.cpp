// Tests of the game sessions of libs/network: a ClientSession and a SessionHost talking through a fake
// network in memory, with the time given by the test (docs/protocol/game.md, sections 3, 7 and 8).

#include "rtype/network/Session.hpp"

#include "rtype/network/DatagramBody.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/UdpSocket.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace rtype::network;
using namespace std::chrono_literals;
using Clock = MessageChannel::Clock;

namespace
{

constexpr std::uint32_t SessionId = 42;
constexpr std::uint8_t InputType = 0x40;
constexpr std::uint8_t SnapshotType = 0x80;
constexpr std::uint8_t EventType = 0x81;

const Address ClientAddress = Address::ipv4({192, 168, 1, 20}, 50000);
const Address GameAddress = Address::ipv4({203, 0, 113, 5}, 4243);

std::vector<std::byte> bytesOf(std::string_view text)
{
    std::vector<std::byte> bytes;
    for (const char c : text) {
        bytes.push_back(static_cast<std::byte>(c));
    }
    return bytes;
}

std::string textOf(std::span<const std::byte> bytes)
{
    std::string text;
    for (const std::byte byte : bytes) {
        text.push_back(static_cast<char>(byte));
    }
    return text;
}

struct Packet {
    Address from;
    Address to;
    std::vector<std::byte> data;
};

// A client and a game, and the datagrams between them. Each step advances the time, updates both sides, then
// delivers what they sent
struct Game {
    const DatagramKey clientKey = DatagramKey::generate();
    const DatagramKey serverKey = DatagramKey::generate();
    Clock::time_point now{};

    std::vector<Packet> inFlight;
    std::vector<Packet> toGame;   // every datagram sent to the game, kept for the tests
    std::vector<Packet> toClient; // every datagram sent to the client
    Address clientAddress = ClientAddress;
    bool dropToGame = false;
    bool dropToClient = false;
    double loss = 0.0;
    std::mt19937 random{5};

    // Client
    ClientSession client{SessionId, clientKey, serverKey, [this](std::span<const std::byte> datagram) {
                             push({clientAddress, GameAddress, {datagram.begin(), datagram.end()}});
                         }};
    std::optional<std::string> accepted;
    std::vector<std::string> clientReceived;
    std::optional<SessionEnd> clientEnd;

    // Game
    std::vector<std::uint32_t> connected;
    std::vector<std::string> gameReceived;
    std::optional<SessionEnd> gameEnd;
    std::function<void(std::uint32_t, std::uint8_t, std::span<const std::byte>)> onGameMessage;
    SessionHost host{
        [this](const Address &to, std::span<const std::byte> datagram) {
            push({GameAddress, to, {datagram.begin(), datagram.end()}});
        },
        {
            .onConnected =
                [this](std::uint32_t session, const Address &) {
                    connected.push_back(session);
                    host.send(session, session_message::Accepted, bytesOf("slot 0"), Delivery::Reliable);
                },
            .onMessage =
                [this](std::uint32_t session, std::uint8_t type, std::span<const std::byte> payload) {
                    gameReceived.push_back(textOf(payload));
                    if (onGameMessage) {
                        onGameMessage(session, type, payload);
                    }
                },
            .onClosed = [this](std::uint32_t, SessionEnd end) { gameEnd = end; },
        }};

    void push(Packet packet)
    {
        (packet.to == GameAddress ? toGame : toClient).push_back(packet);
        std::uniform_real_distribution<double> chance(0.0, 1.0);
        const bool dropped = (packet.to == GameAddress ? dropToGame : dropToClient) || chance(random) < loss;
        if (!dropped) {
            inFlight.push_back(std::move(packet));
        }
    }

    void deliver()
    {
        std::vector<Packet> packets;
        packets.swap(inFlight);
        for (const Packet &packet : packets) {
            if (packet.to == GameAddress) {
                host.receive(packet.from, packet.data, now);
            } else {
                client.receive(packet.data, now);
            }
        }
    }

    void step(Clock::duration elapsed = 16ms)
    {
        now += elapsed;
        client.update(now);
        host.update(now);
        deliver();
    }

    void run(Clock::duration duration, Clock::duration elapsed = 16ms)
    {
        const Clock::time_point end = now + duration;
        while (now < end) {
            step(elapsed);
        }
    }

    void startClient()
    {
        client.start([this](std::span<const std::byte> payload) { accepted = textOf(payload); },
                     [this](std::uint8_t, std::span<const std::byte> payload) {
                         clientReceived.push_back(textOf(payload));
                     },
                     [this](SessionEnd end) { clientEnd = end; }, now);
    }

    void connect()
    {
        host.add(SessionId, clientKey, serverKey, now);
        startClient();
        for (int i = 0; i < 10 && client.state() != ClientSession::State::Connected; ++i) {
            step();
        }
        REQUIRE(client.state() == ClientSession::State::Connected);
    }
};

} // namespace

// --- Connecting ---

TEST_CASE("A client connects to its game and both exchange messages", "[network]")
{
    Game game;
    game.host.add(SessionId, game.clientKey, game.serverKey, game.now);
    CHECK_FALSE(game.host.connected(SessionId));
    game.startClient();
    CHECK(game.client.state() == ClientSession::State::Connecting);
    CHECK_FALSE(game.client.send(InputType, bytesOf("too early"), Delivery::Unreliable));

    game.step();
    CHECK(game.connected == std::vector<std::uint32_t>{SessionId});
    CHECK(game.host.address(SessionId) == ClientAddress);
    game.step();
    CHECK(game.client.state() == ClientSession::State::Connected);
    CHECK(game.accepted == "slot 0");

    CHECK(game.client.send(InputType, bytesOf("up"), Delivery::Unreliable));
    CHECK(game.host.send(SessionId, SnapshotType, bytesOf("world"), Delivery::Unreliable));
    CHECK(game.host.send(SessionId, EventType, bytesOf("wave 1"), Delivery::Reliable));
    game.step();
    game.step();

    CHECK(game.gameReceived == std::vector<std::string>{"up"});
    CHECK(game.clientReceived == std::vector<std::string>{"wave 1", "world"});
    CHECK_FALSE(game.clientEnd);
    CHECK_FALSE(game.gameEnd);
}

TEST_CASE("A session is added once, with an id that is not 0", "[network]")
{
    Game game;
    game.host.add(SessionId, game.clientKey, game.serverKey, game.now);

    CHECK_THROWS_AS(game.host.add(SessionId, game.clientKey, game.serverKey, game.now),
                    std::invalid_argument);
    CHECK_THROWS_AS(game.host.add(0, game.clientKey, game.serverKey, game.now), std::invalid_argument);
    CHECK(game.host.size() == 1);
    CHECK_FALSE(game.host.send(SessionId, SnapshotType, bytesOf("not connected"), Delivery::Unreliable));
    CHECK_FALSE(game.host.send(7, SnapshotType, bytesOf("unknown"), Delivery::Unreliable));
}

TEST_CASE("CONNECT is sent again until the game answers", "[network]")
{
    Game game;
    game.host.add(SessionId, game.clientKey, game.serverKey, game.now);
    game.dropToGame = true;
    game.startClient();

    game.run(1s);
    CHECK(game.toGame.size() > 3);
    CHECK(game.connected.empty());

    game.dropToGame = false;
    game.run(500ms);
    CHECK(game.connected == std::vector<std::uint32_t>{SessionId});
    CHECK(game.client.state() == ClientSession::State::Connected);
}

TEST_CASE("A client that gets no ACCEPTED gives up after the connect timeout", "[network]")
{
    Game game;
    game.startClient(); // no session on the game: everything is dropped

    game.run(ConnectTimeout - 100ms);
    CHECK_FALSE(game.clientEnd);
    game.run(200ms);

    REQUIRE(game.clientEnd);
    CHECK(game.clientEnd->cause == SessionEnd::Cause::ConnectTimedOut);
    CHECK(game.client.state() == ClientSession::State::Closed);
    // The game never answered anything, not even to say the session is unknown
    CHECK(game.toClient.empty());
}

TEST_CASE("A game forgets a session whose client never connects", "[network]")
{
    Game game;
    game.host.add(SessionId, game.clientKey, game.serverKey, game.now);

    game.run(PendingTimeout - 100ms, 100ms);
    CHECK(game.host.contains(SessionId));
    game.run(200ms, 100ms);

    REQUIRE(game.gameEnd);
    CHECK(game.gameEnd->cause == SessionEnd::Cause::ConnectTimedOut);
    CHECK_FALSE(game.host.contains(SessionId));
    CHECK(game.toClient.empty());
}

// --- Staying connected ---

TEST_CASE("Heartbeats keep an idle session open", "[network]")
{
    Game game;
    game.connect();
    const std::size_t before = game.toGame.size() + game.toClient.size();

    game.run(20s, 100ms);

    CHECK_FALSE(game.clientEnd);
    CHECK_FALSE(game.gameEnd);
    // About one datagram per second each way, not one per update
    const std::size_t heartbeats = game.toGame.size() + game.toClient.size() - before;
    CHECK(heartbeats >= 38);
    CHECK(heartbeats <= 44);
    REQUIRE(game.client.rtt());
    REQUIRE(game.host.rtt(SessionId));
}

TEST_CASE("A session silent for 5 seconds is closed on both sides", "[network]")
{
    Game game;
    game.connect();
    game.dropToGame = true;
    game.dropToClient = true;

    game.run(SessionTimeout - 200ms);
    CHECK_FALSE(game.clientEnd);
    CHECK_FALSE(game.gameEnd);
    game.run(300ms);

    REQUIRE(game.clientEnd);
    CHECK(game.clientEnd->cause == SessionEnd::Cause::TimedOut);
    REQUIRE(game.gameEnd);
    CHECK(game.gameEnd->cause == SessionEnd::Cause::TimedOut);
    CHECK_FALSE(game.host.contains(SessionId));
}

TEST_CASE("Over a network that loses 30 % of the datagrams, the session connects and delivers", "[network]")
{
    Game game;
    game.loss = 0.3;
    game.host.add(SessionId, game.clientKey, game.serverKey, game.now);
    game.startClient();
    game.run(2s);
    REQUIRE(game.client.state() == ClientSession::State::Connected);

    for (int i = 0; i < 20; ++i) {
        REQUIRE(game.host.send(SessionId, EventType, bytesOf(std::to_string(i)), Delivery::Reliable));
        game.step();
    }
    game.run(3s);

    std::vector<std::string> expected;
    for (int i = 0; i < 20; ++i) {
        expected.push_back(std::to_string(i));
    }
    CHECK(game.clientReceived == expected);
    CHECK_FALSE(game.clientEnd);
    CHECK_FALSE(game.gameEnd);
}

// --- Closing ---

TEST_CASE("A client that leaves sends DISCONNECT, and the game closes the session", "[network]")
{
    Game game;
    game.connect();
    const std::size_t before = game.toGame.size();

    game.client.close(DisconnectReason::Quit);
    CHECK(game.client.state() == ClientSession::State::Closed);
    CHECK(game.toGame.size() == before + DisconnectRepeats);
    game.step();

    REQUIRE(game.gameEnd);
    CHECK(*game.gameEnd == SessionEnd{SessionEnd::Cause::Disconnected, DisconnectReason::Quit});
    CHECK_FALSE(game.host.contains(SessionId));
    // Closed by the application: its own handler is not called
    CHECK_FALSE(game.clientEnd);
}

TEST_CASE("A game that ends sends DISCONNECT with the reason, and the client closes", "[network]")
{
    Game game;
    game.connect();

    game.host.close(SessionId, DisconnectReason::GameOver);
    CHECK_FALSE(game.host.contains(SessionId));
    game.step();

    REQUIRE(game.clientEnd);
    CHECK(*game.clientEnd == SessionEnd{SessionEnd::Cause::Disconnected, DisconnectReason::GameOver});
    CHECK_FALSE(game.gameEnd);
}

TEST_CASE("One DISCONNECT out of three is enough", "[network]")
{
    Game game;
    game.connect();

    game.host.closeAll(DisconnectReason::Shutdown);
    REQUIRE(game.inFlight.size() == DisconnectRepeats);
    game.inFlight.erase(game.inFlight.begin(), game.inFlight.begin() + 2);
    game.step();

    REQUIRE(game.clientEnd);
    CHECK(game.clientEnd->reason == DisconnectReason::Shutdown);
}

TEST_CASE("A game that stops acknowledging makes the client close with a protocol error", "[network]")
{
    Game game;
    game.connect();
    game.dropToClient = true; // the client receives nothing, the game still hears it

    bool refused = false;
    for (int i = 0; i < 300 && !refused; ++i) {
        refused = !game.client.send(EventType, bytesOf("action"), Delivery::Reliable);
    }
    CHECK(refused);
    game.dropToClient = false;
    game.step();

    REQUIRE(game.clientEnd);
    CHECK(game.clientEnd->cause == SessionEnd::Cause::Failed);
    game.step();
    REQUIRE(game.gameEnd);
    CHECK(*game.gameEnd == SessionEnd{SessionEnd::Cause::Disconnected, DisconnectReason::ProtocolError});
}

TEST_CASE("A handler can close the session it is called for", "[network]")
{
    Game game;
    game.connect();
    game.onGameMessage = [&](std::uint32_t session, std::uint8_t, std::span<const std::byte>) {
        game.host.close(session, DisconnectReason::ProtocolError);
    };

    game.client.send(InputType, bytesOf("first"), Delivery::Unreliable);
    game.client.send(InputType, bytesOf("second"), Delivery::Unreliable);
    game.step();
    game.step();

    CHECK(game.gameReceived == std::vector<std::string>{"first"});
    CHECK_FALSE(game.host.contains(SessionId));
    REQUIRE(game.clientEnd);
    CHECK(game.clientEnd->reason == DisconnectReason::ProtocolError);
}

// --- Attacks and accidents ---

TEST_CASE("Datagrams of unknown sessions, of other keys or replayed are dropped silently", "[network]")
{
    Game game;
    game.connect();
    game.client.send(InputType, bytesOf("move"), Delivery::Unreliable);
    game.client.update(game.now);
    const Packet recorded = game.inFlight.back();
    game.deliver();
    REQUIRE(game.gameReceived == std::vector<std::string>{"move"});
    const std::size_t answers = game.toClient.size();

    // Replayed
    game.host.receive(recorded.from, recorded.data, game.now);
    // Another key
    const DatagramKey other = DatagramKey::generate();
    const std::vector<std::byte> body = encodeBody({0, 0, {{InputType, false, 0, bytesOf("forged")}}});
    game.host.receive(ClientAddress, sealDatagram(other, {SessionId, 1000}, body), game.now);
    // Unknown session, garbage
    game.host.receive(ClientAddress, sealDatagram(game.clientKey, {7, 1000}, body), game.now);
    game.host.receive(ClientAddress, bytesOf("not a datagram at all"), game.now);

    CHECK(game.gameReceived == std::vector<std::string>{"move"});
    // No answer to anything that failed: it could be used to flood someone else
    CHECK(game.toClient.size() == answers);
    CHECK(game.host.connected(SessionId));
}

TEST_CASE("A client whose port changes is followed by the game", "[network]")
{
    Game game;
    game.connect();
    const Address moved = Address::ipv4({192, 168, 1, 20}, 61000);

    game.clientAddress = moved;
    game.client.send(InputType, bytesOf("after the router"), Delivery::Unreliable);
    game.step();

    CHECK(game.host.address(SessionId) == moved);
    game.host.send(SessionId, SnapshotType, bytesOf("world"), Delivery::Unreliable);
    game.step();
    CHECK(game.toClient.back().to == moved);
    CHECK(game.clientReceived == std::vector<std::string>{"world"});
}

TEST_CASE("An older datagram from another address does not move the session", "[network]")
{
    Game game;
    game.connect();
    game.client.send(InputType, bytesOf("old"), Delivery::Unreliable);
    game.client.update(game.now);
    Packet old = game.inFlight.back();
    game.inFlight.pop_back();
    game.client.send(InputType, bytesOf("new"), Delivery::Unreliable);
    game.step();

    // The old datagram arrives late, through another address: delivered, but the session stays
    old.from = Address::ipv4({198, 51, 100, 9}, 40000);
    game.host.receive(old.from, old.data, game.now);

    CHECK(game.gameReceived == std::vector<std::string>{"new", "old"});
    CHECK(game.host.address(SessionId) == ClientAddress);
}

TEST_CASE("A source address sending more than 200 datagrams per second is cut", "[network]")
{
    Game game;
    game.connect();
    game.now += 2s;

    // 250 authentic datagrams in the same second, from the client's address
    MessageChannel forger;
    for (int i = 0; i < 250; ++i) {
        forger.send(InputType, bytesOf("spam"), Delivery::Unreliable);
        const auto datagram = forger.next(game.now);
        REQUIRE(datagram);
        // Numbers above those of the client, so that the replay window accepts them
        const auto sealed =
            sealDatagram(game.clientKey, {SessionId, 100000 + datagram->packetNumber}, datagram->body);
        game.host.receive(ClientAddress, sealed, game.now);
    }

    CHECK(game.gameReceived.size() == MaxDatagramsPerSecond);
    // The next second, the address is heard again
    game.now += 1s;
    forger.send(InputType, bytesOf("later"), Delivery::Unreliable);
    const auto datagram = forger.next(game.now);
    game.host.receive(ClientAddress, sealDatagram(game.clientKey, {SessionId, 200000}, datagram->body),
                      game.now);
    CHECK(game.gameReceived.back() == "later");
}

// --- Over real sockets ---

TEST_CASE("A session works over UDP sockets on the loopback", "[network]")
{
    EventLoop loop;
    UdpSocket gameSocket(loop, Address::loopbackIpv4(0));
    UdpSocket clientSocket(loop, Address::loopbackIpv4(0));
    const Address game = gameSocket.localAddress();
    const DatagramKey clientKey = DatagramKey::generate();
    const DatagramKey serverKey = DatagramKey::generate();
    std::vector<std::string> received;

    SessionHost host(
        [&](const Address &to, std::span<const std::byte> datagram) { gameSocket.send(to, datagram); },
        {
            .onConnected =
                [&](std::uint32_t session, const Address &) {
                    host.send(session, session_message::Accepted, {}, Delivery::Reliable);
                },
            .onMessage = [&](std::uint32_t, std::uint8_t,
                             std::span<const std::byte> payload) { received.push_back(textOf(payload)); },
            .onClosed = {},
        });
    gameSocket.startReceiving([&](const Address &from, std::span<const std::byte> datagram) {
        host.receive(from, datagram, Clock::now());
    });
    host.add(SessionId, clientKey, serverKey, Clock::now());

    ClientSession client(SessionId, clientKey, serverKey,
                         [&](std::span<const std::byte> datagram) { clientSocket.send(game, datagram); });
    clientSocket.startReceiving([&](const Address &from, std::span<const std::byte> datagram) {
        if (from == game) {
            client.receive(datagram, Clock::now());
        }
    });
    bool accepted = false;
    client.start([&](std::span<const std::byte>) { accepted = true; }, {}, {}, Clock::now());

    // The game loop of a client: poll, send, update
    const auto deadline = Clock::now() + 5s;
    bool sent = false;
    while (received.empty() && Clock::now() < deadline) {
        loop.poll();
        if (accepted && !sent) {
            sent = client.send(InputType, bytesOf("hello over UDP"), Delivery::Reliable);
        }
        client.update(Clock::now());
        host.update(Clock::now());
        std::this_thread::sleep_for(1ms);
    }

    CHECK(accepted);
    CHECK(received == std::vector<std::string>{"hello over UDP"});
    CHECK(host.address(SessionId) == clientSocket.localAddress());
}
