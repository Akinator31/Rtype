// Tests of the lobby of the master: real TLS clients, pinning the key of the server, that say hello, list the
// games, ping, break the rules, and hit the limits (docs/protocol/lobby.md, docs/architecture/server.md).

#include "Master/Lobby.hpp"
#include "rtype/lobby/Messages.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/FramedConnection.hpp"
#include "rtype/network/TcpConnection.hpp"
#include "rtype/network/TcpListener.hpp"
#include "rtype/network/Tls.hpp"
#include "rtype/network/TlsConnection.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

using namespace rtype::server;
using namespace std::chrono_literals;
using namespace rtype::lobby;
using rtype::network::Address;
using rtype::network::EventLoop;
using rtype::network::FramedConnection;
using rtype::network::KeyFingerprint;
using rtype::network::ServerIdentity;
using rtype::network::TcpConnection;
using rtype::network::TcpListener;
using rtype::network::TlsConnection;
using Clock = std::chrono::steady_clock;
using Bytes = std::vector<std::byte>;

namespace
{

constexpr auto Patience = 10s;

bool waitUntil(EventLoop &loop, const std::function<bool()> &condition,
               std::chrono::milliseconds limit = Patience)
{
    const auto deadline = Clock::now() + limit;
    while (!condition()) {
        if (Clock::now() > deadline) {
            return false;
        }
        loop.poll();
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

/// Lets the loop run for a while, for what must not happen.
void runFor(EventLoop &loop, std::chrono::milliseconds duration)
{
    (void)waitUntil(loop, [] { return false; }, duration);
}

/// A client of the lobby: TCP, then TLS pinning the key of the server, then frames.
struct LobbyClient {
    std::optional<FramedConnection> connection;
    std::vector<std::pair<std::uint8_t, Bytes>> received;
    std::optional<std::error_code> closed;
    bool secured = false;

    LobbyClient(EventLoop &loop, const Address &server, const KeyFingerprint &pinned)
    {
        TcpConnection::connect(
            loop, server, [this, pinned](std::error_code error, std::optional<TcpConnection> tcp) {
                if (error || !tcp) {
                    closed = error;
                    return;
                }
                connection.emplace(TlsConnection::client(std::move(*tcp), pinned));
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
        send(message_type::Hello, encode(HelloMessage{ProtocolVersion, name}));
    }

    /// The reply number `index`, once received.
    std::optional<std::pair<std::uint8_t, Bytes>> reply(EventLoop &loop, std::size_t index = 0)
    {
        if (!waitUntil(loop, [&] { return received.size() > index; })) {
            return std::nullopt;
        }
        return received[index];
    }

    /// The error of reply number `index`, if it is one.
    std::optional<ErrorMessage> error(EventLoop &loop, std::size_t index = 0)
    {
        const auto answer = reply(loop, index);
        if (!answer || answer->first != message_type::Error) {
            return std::nullopt;
        }
        return decodeError(answer->second);
    }
};

/// A lobby on the loopback, and its clients.
struct LobbyFixture {
    EventLoop loop;
    ServerIdentity identity = ServerIdentity::generate();
    Lobby lobby;

    explicit LobbyFixture(LobbyLimits limits = {})
        : lobby(loop, TcpListener(loop, Address::loopbackIpv4(0)), identity, limits)
    {
    }

    std::unique_ptr<LobbyClient> client()
    {
        auto created = std::make_unique<LobbyClient>(loop, lobby.localAddress(), identity.fingerprint());
        REQUIRE(
            waitUntil(loop, [&] { return created->connection.has_value() || created->closed.has_value(); }));
        return created;
    }

    /// A client welcomed in the lobby.
    std::unique_ptr<LobbyClient> player(const std::string &name)
    {
        auto created = client();
        created->hello(name);
        REQUIRE(created->reply(loop));
        REQUIRE(created->received[0].first == message_type::Welcome);
        return created;
    }
};

} // namespace

// --- The lobby ---

TEST_CASE("A client says hello and gets an id, then lists the games and pings", "[lobby]")
{
    LobbyFixture fixture;
    auto alice = fixture.client();
    auto bob = fixture.client();

    alice->hello("Alice");
    bob->hello("Bob");
    const auto aliceWelcome = alice->reply(fixture.loop);
    const auto bobWelcome = bob->reply(fixture.loop);
    REQUIRE(aliceWelcome);
    REQUIRE(bobWelcome);
    REQUIRE(aliceWelcome->first == message_type::Welcome);
    const std::uint32_t aliceId = decodeWelcome(aliceWelcome->second)->playerId;
    const std::uint32_t bobId = decodeWelcome(bobWelcome->second)->playerId;
    CHECK(aliceId != 0);
    CHECK(bobId != 0);
    CHECK(aliceId != bobId);
    CHECK(alice->secured);

    alice->send(message_type::ListGames);
    alice->send(message_type::Ping);
    const auto list = alice->reply(fixture.loop, 1);
    REQUIRE(list);
    CHECK(list->first == message_type::GameList);
    CHECK(decodeGameList(list->second) == GameListMessage{});
    REQUIRE(alice->reply(fixture.loop, 2));
    CHECK(alice->received[2] == std::pair<std::uint8_t, Bytes>{message_type::Pong, {}});
    CHECK(fixture.lobby.connectionCount() == 2);
}

TEST_CASE("The replies come in the order of the requests, sent without waiting", "[lobby]")
{
    LobbyFixture fixture;
    auto client = fixture.client();

    client->send(message_type::Ping);
    client->hello("Pavel");
    client->send(message_type::ListGames);
    client->send(message_type::JoinGame, encode(JoinGameMessage{7}));
    client->send(message_type::Ping);

    REQUIRE(client->reply(fixture.loop, 4));
    std::vector<std::uint8_t> types;
    for (const auto &[type, payload] : client->received) {
        types.push_back(type);
    }
    CHECK(types == std::vector<std::uint8_t>{message_type::Pong, message_type::Welcome,
                                             message_type::GameList, message_type::Error,
                                             message_type::Pong});
}

TEST_CASE("Before HELLO, only HELLO and PING are answered", "[lobby]")
{
    LobbyFixture fixture;
    auto client = fixture.client();

    client->send(message_type::ListGames);
    client->send(message_type::LeaveGame);
    client->send(message_type::Ping);

    CHECK(client->error(fixture.loop, 0) == ErrorMessage{message_type::ListGames, ErrorCode::HelloRequired});
    CHECK(client->error(fixture.loop, 1) == ErrorMessage{message_type::LeaveGame, ErrorCode::HelloRequired});
    REQUIRE(client->reply(fixture.loop, 2));
    CHECK(client->received[2].first == message_type::Pong);
    // Kept: the client can still say hello
    client->hello("Pavel");
    REQUIRE(client->reply(fixture.loop, 3));
    CHECK(client->received[3].first == message_type::Welcome);
}

TEST_CASE("In the lobby, each request gets the answer of its state", "[lobby]")
{
    LobbyFixture fixture;
    auto client = fixture.player("Pavel");

    client->hello("Pavel");
    client->send(message_type::LeaveGame);
    client->send(message_type::JoinGame, encode(JoinGameMessage{7}));
    client->send(message_type::CreateGame, encode(CreateGameMessage{"Bydo hunters", 4}));
    client->send(0x42);
    client->send(message_type::Welcome, encode(WelcomeMessage{1}));

    CHECK(client->error(fixture.loop, 1) == ErrorMessage{message_type::Hello, ErrorCode::AlreadyWelcomed});
    CHECK(client->error(fixture.loop, 2) == ErrorMessage{message_type::LeaveGame, ErrorCode::NotInGame});
    CHECK(client->error(fixture.loop, 3) == ErrorMessage{message_type::JoinGame, ErrorCode::GameNotFound});
    // No game can start before `server: games`
    CHECK(client->error(fixture.loop, 4) ==
          ErrorMessage{message_type::CreateGame, ErrorCode::GameStartFailed});
    CHECK(client->error(fixture.loop, 5) == ErrorMessage{0x42, ErrorCode::UnknownMessage});
    CHECK(client->error(fixture.loop, 6) == ErrorMessage{message_type::Welcome, ErrorCode::UnknownMessage});
    // Still open
    client->send(message_type::Ping);
    REQUIRE(client->reply(fixture.loop, 7));
    CHECK_FALSE(client->closed);
}

TEST_CASE("An invalid name is refused, and the client can try again", "[lobby]")
{
    LobbyFixture fixture;
    auto client = fixture.client();
    const auto sendName = [&](std::string name) {
        Bytes payload = {std::byte{0}, std::byte{1}, static_cast<std::byte>(name.size())};
        for (const char character : name) {
            payload.push_back(static_cast<std::byte>(character));
        }
        client->send(message_type::Hello, payload);
    };

    sendName("");
    sendName(std::string(17, 'p'));
    sendName("bell\a");
    sendName("\xFF\xFE");
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(client->error(fixture.loop, i) == ErrorMessage{message_type::Hello, ErrorCode::InvalidName});
    }
    client->hello("Pavel");
    REQUIRE(client->reply(fixture.loop, 4));
    CHECK(client->received[4].first == message_type::Welcome);

    client->send(message_type::CreateGame, Bytes{std::byte{0}, std::byte{2}});
    CHECK(client->error(fixture.loop, 5) == ErrorMessage{message_type::CreateGame, ErrorCode::InvalidName});
}

TEST_CASE("Another version of the protocol is refused, and the connection closed", "[lobby]")
{
    LobbyFixture fixture;
    auto client = fixture.client();

    client->send(message_type::Hello, encode(HelloMessage{2, "Pavel"}));

    CHECK(client->error(fixture.loop) == ErrorMessage{message_type::Hello, ErrorCode::UnsupportedVersion});
    CHECK(waitUntil(fixture.loop, [&] { return client->closed.has_value(); }));
    CHECK(waitUntil(fixture.loop, [&] { return fixture.lobby.connectionCount() == 0; }));
}

TEST_CASE("A malformed request gets MALFORMED, and the connection is closed", "[lobby]")
{
    SECTION("HELLO, checked before the client is welcomed")
    {
        LobbyFixture fixture;
        auto client = fixture.client();

        client->send(message_type::Hello, Bytes{std::byte{0}});

        CHECK(client->error(fixture.loop) == ErrorMessage{message_type::Hello, ErrorCode::Malformed});
        CHECK(waitUntil(fixture.loop, [&] { return client->closed.has_value(); }));
    }
    SECTION("The other requests, once welcomed")
    {
        // A second HELLO is ALREADY_WELCOMED, whatever it holds: the state is checked first
        const std::vector<std::pair<std::uint8_t, Bytes>> requests = {
            {message_type::Ping, Bytes{std::byte{0}}},
            {message_type::ListGames, Bytes{std::byte{0}}},
            {message_type::CreateGame, Bytes{std::byte{1}, std::byte{'A'}, std::byte{5}}},
            {message_type::JoinGame, Bytes{std::byte{1}}},
            {message_type::LeaveGame, Bytes{std::byte{0}}},
        };
        for (const auto &[type, payload] : requests) {
            INFO("request " << static_cast<int>(type));
            LobbyFixture fixture;
            auto client = fixture.player("Pavel");

            client->send(type, payload);

            CHECK(client->error(fixture.loop, 1) == ErrorMessage{type, ErrorCode::Malformed});
            CHECK(waitUntil(fixture.loop, [&] { return client->closed.has_value(); }));
        }
    }
}

// --- The limits ---

TEST_CASE("Connections beyond the limits are closed at once", "[lobby]")
{
    SECTION("Per IP address")
    {
        LobbyFixture fixture({.maxConnections = 10, .maxConnectionsPerAddress = 2});
        auto first = fixture.player("One");
        auto second = fixture.player("Two");

        auto third = fixture.client();

        CHECK(waitUntil(fixture.loop, [&] { return third->closed.has_value(); }));
        CHECK_FALSE(third->secured);
        CHECK(fixture.lobby.connectionCount() == 2);
        // A place freed is taken again
        first->connection->close();
        REQUIRE(waitUntil(fixture.loop, [&] { return fixture.lobby.connectionCount() == 1; }));
        auto fourth = fixture.player("Four");
        CHECK(fixture.lobby.connectionCount() == 2);
    }
    SECTION("In all")
    {
        LobbyFixture fixture({.maxConnections = 1});
        auto first = fixture.player("One");

        auto second = fixture.client();

        CHECK(waitUntil(fixture.loop, [&] { return second->closed.has_value(); }));
        CHECK_FALSE(second->secured);
        CHECK(fixture.lobby.connectionCount() == 1);
    }
}

TEST_CASE("A connection that does not finish its TLS handshake in time is closed", "[lobby]")
{
    LobbyFixture fixture({.handshakeTimeout = 200ms});
    std::optional<TcpConnection> raw;
    bool closed = false;
    TcpConnection::connect(fixture.loop, fixture.lobby.localAddress(),
                           [&](std::error_code, std::optional<TcpConnection> connection) {
                               raw = std::move(connection);
                               raw->start([](std::span<const std::byte>) {},
                                          [&](std::error_code) { closed = true; });
                           });
    const auto start = Clock::now();

    // Says nothing: no ClientHello
    REQUIRE(waitUntil(fixture.loop, [&] { return closed; }));
    CHECK(Clock::now() - start >= 150ms);
    CHECK(waitUntil(fixture.loop, [&] { return fixture.lobby.connectionCount() == 0; }));
}

TEST_CASE("A client that does not say hello in time is closed", "[lobby]")
{
    LobbyFixture fixture({.helloTimeout = 200ms});
    auto client = fixture.client();
    REQUIRE(waitUntil(fixture.loop, [&] { return client->secured; }));

    // PING does not count: only HELLO does
    client->send(message_type::Ping);

    REQUIRE(waitUntil(fixture.loop, [&] { return client->closed.has_value(); }));
    CHECK(client->received.size() == 1);
}

TEST_CASE("A silent client is closed, one that pings stays", "[lobby]")
{
    LobbyFixture fixture({.idleTimeout = 300ms});
    auto silent = fixture.player("Silent");
    auto pinging = fixture.player("Pinging");

    const auto start = Clock::now();
    while (Clock::now() - start < 700ms) {
        pinging->send(message_type::Ping);
        runFor(fixture.loop, 100ms);
    }

    CHECK(silent->closed.has_value());
    CHECK_FALSE(pinging->closed.has_value());
    CHECK(fixture.lobby.connectionCount() == 1);
}

TEST_CASE("Closing the lobby disconnects its clients and stops accepting", "[lobby]")
{
    LobbyFixture fixture;
    auto alice = fixture.player("Alice");
    auto unwelcomed = fixture.client();
    REQUIRE(waitUntil(fixture.loop, [&] { return unwelcomed->secured; }));

    fixture.lobby.close();

    REQUIRE(
        waitUntil(fixture.loop, [&] { return alice->closed.has_value() && unwelcomed->closed.has_value(); }));
    // Closed properly, with TLS close_notify: no error
    CHECK_FALSE(*alice->closed);
    CHECK(fixture.lobby.connectionCount() == 0);
    auto late = std::make_unique<LobbyClient>(fixture.loop, fixture.lobby.localAddress(),
                                              fixture.identity.fingerprint());
    CHECK(waitUntil(fixture.loop, [&] { return late->closed.has_value(); }));
}
