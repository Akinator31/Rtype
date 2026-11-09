// Tests of the lobby of the master: real TLS clients, pinning the key of the server, that say hello, list the
// games, ping, break the rules, and hit the limits (docs/protocol/lobby.md, docs/architecture/server.md).

#include "LobbyClient.hpp"
#include "Master/Games.hpp"
#include "Master/Lobby.hpp"
#include "Master/Relay.hpp"
#include "rtype/lobby/Messages.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/FramedConnection.hpp"
#include "rtype/network/TcpConnection.hpp"
#include "rtype/network/TcpListener.hpp"
#include "rtype/network/Tls.hpp"
#include "rtype/network/TlsConnection.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
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
using Catch::Matchers::ContainsSubstring;
using testing::LobbyClient;
using testing::runFor;
using testing::waitUntil;

namespace
{

/// Captures the messages logged during a test, at every level.
class Capture : public rtype::log::Sink
{
private:
    mutable std::mutex _mutex;
    std::string _text;

public:
    void write(const rtype::log::Record &record) override
    {
        const std::lock_guard lock(_mutex);
        _text.append(record.message).append("\n");
    }

    [[nodiscard]] std::string text() const
    {
        const std::lock_guard lock(_mutex);
        return _text;
    }
};

struct Captured {
    std::shared_ptr<Capture> sink = std::make_shared<Capture>();
    rtype::log::Level previous = rtype::log::level();

    Captured()
    {
        rtype::log::setSink(sink);
        rtype::log::setLevel(rtype::log::Level::Trace);
    }
    ~Captured()
    {
        rtype::log::setLevel(previous);
        rtype::log::setSink(nullptr);
    }
    Captured(const Captured &) = delete;
    Captured &operator=(const Captured &) = delete;
};

/// A session token no master gave.
SessionToken someToken()
{
    SessionToken token{};
    token.fill(std::byte{0x5A});
    return token;
}

/// One valid request of each kind of the accounts.
std::vector<std::pair<std::uint8_t, Bytes>> accountRequests()
{
    return {
        {message_type::Register,
         encode(RegisterMessage{"Pavel", "pavel@example.org", "correct horse battery"})},
        {message_type::VerifyEmail, encode(VerifyEmailMessage{"Pavel", "123456"})},
        {message_type::ResendCode, encode(ResendCodeMessage{"Pavel"})},
        {message_type::SignIn, encode(SignInMessage{"Pavel", "correct horse battery"})},
        {message_type::SignOut, {}},
        {message_type::ForgotPassword, encode(ForgotPasswordMessage{"pavel@example.org"})},
        {message_type::ResetPassword,
         encode(ResetPasswordMessage{"pavel@example.org", "123456", "new password!"})},
        {message_type::DiscordSignIn, {}},
        {message_type::DiscordConfirm,
         encode(DiscordConfirmMessage{"code", std::string(43, 'v'), "http://127.0.0.1/callback"})},
        {message_type::ChooseUsername, encode(ChooseUsernameMessage{"Pavel"})},
        {message_type::DeleteAccount, encode(DeleteAccountMessage{"correct horse battery"})},
    };
}

/// A lobby on the loopback, and its clients.
struct LobbyFixture {
    EventLoop loop;
    ServerIdentity identity = ServerIdentity::generate();
    /// No game is created here: tests/server_games.cpp creates them
    Relay relay{Address::loopbackIpv4(0)};
    Games games{
        loop, relay,
        GamesSettings{.executable = RTYPE_TEST_CHILD, .leadingArguments = {}, .trailingArguments = {}}};
    Lobby lobby;

    explicit LobbyFixture(LobbyLimits limits = {})
        : lobby(loop, TcpListener(loop, Address::loopbackIpv4(0)), identity, games, limits)
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
    client->send(0x42);
    client->send(message_type::Welcome,
                 encode(WelcomeMessage{.playerId = 1, .features = {}, .account = std::nullopt}));

    CHECK(client->error(fixture.loop, 1) == ErrorMessage{message_type::Hello, ErrorCode::AlreadyWelcomed});
    CHECK(client->error(fixture.loop, 2) == ErrorMessage{message_type::LeaveGame, ErrorCode::NotInGame});
    CHECK(client->error(fixture.loop, 3) == ErrorMessage{message_type::JoinGame, ErrorCode::GameNotFound});
    CHECK(client->error(fixture.loop, 4) == ErrorMessage{0x42, ErrorCode::UnknownMessage});
    CHECK(client->error(fixture.loop, 5) == ErrorMessage{message_type::Welcome, ErrorCode::UnknownMessage});
    // Still open
    client->send(message_type::Ping);
    REQUIRE(client->reply(fixture.loop, 6));
    CHECK_FALSE(client->closed);
}

TEST_CASE("An invalid name is refused, and the client can try again", "[lobby]")
{
    LobbyFixture fixture;
    auto client = fixture.client();
    const auto sendName = [&](std::string name) {
        Bytes payload = {std::byte{0}, static_cast<std::byte>(ProtocolVersion),
                         static_cast<std::byte>(name.size())};
        for (const char character : name) {
            payload.push_back(static_cast<std::byte>(character));
        }
        // Locale "en", no token
        payload.insert(payload.end(), {std::byte{2}, std::byte{'e'}, std::byte{'n'}, std::byte{0}});
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
    // A client of before the administration (version 1), and one of a newer server
    const std::uint16_t version = GENERATE(std::uint16_t{1}, static_cast<std::uint16_t>(ProtocolVersion + 1));
    CAPTURE(version);
    LobbyFixture fixture;
    auto client = fixture.client();

    client->send(
        message_type::Hello,
        encode(HelloMessage{.version = version, .name = "Pavel", .locale = "en", .token = std::nullopt}));

    CHECK(client->error(fixture.loop) == ErrorMessage{message_type::Hello, ErrorCode::UnsupportedVersion});
    CHECK(waitUntil(fixture.loop, [&] { return client->closed.has_value(); }));
    CHECK(waitUntil(fixture.loop, [&] { return fixture.lobby.connectionCount() == 0; }));
}

TEST_CASE("A client of version 2 is told to update, not that its HELLO is malformed", "[lobby]")
{
    LobbyFixture fixture;
    auto client = fixture.client();

    // HELLO of version 2: a version and a name, without the locale and the token of version 3
    client->send(message_type::Hello, Bytes{std::byte{0}, std::byte{2}, std::byte{3}, std::byte{'B'},
                                            std::byte{'o'}, std::byte{'b'}});

    CHECK(client->error(fixture.loop) == ErrorMessage{message_type::Hello, ErrorCode::UnsupportedVersion});
    CHECK(waitUntil(fixture.loop, [&] { return client->closed.has_value(); }));
}

TEST_CASE("A master without accounts says so in WELCOME, and answers ACCOUNTS_DISABLED", "[lobby][accounts]")
{
    LobbyFixture fixture;
    auto client = fixture.client();
    client->send(message_type::Hello,
                 encode(HelloMessage{
                     .version = ProtocolVersion, .name = "Pavel", .locale = "fr-FR", .token = std::nullopt}));
    REQUIRE(client->reply(fixture.loop));
    REQUIRE(client->received[0].first == message_type::Welcome);
    const auto welcome = decodeWelcome(client->received[0].second);
    REQUIRE(welcome);
    CHECK(welcome->features == ServerFeatures{.guests = true, .passwordAccounts = false, .discord = false});
    CHECK_FALSE(welcome->account);

    // Every request of the accounts, sent without waiting, answered in order; the connection stays
    const auto requests = accountRequests();
    for (const auto &[type, payload] : requests) {
        client->send(type, payload);
    }
    client->send(message_type::Ping);
    for (std::size_t i = 0; i < requests.size(); ++i) {
        CHECK(client->error(fixture.loop, i + 1) ==
              ErrorMessage{requests[i].first, ErrorCode::AccountsDisabled});
    }
    REQUIRE(client->reply(fixture.loop, requests.size() + 1));
    CHECK(client->received.back().first == message_type::Pong);
    CHECK_FALSE(client->closed);
}

TEST_CASE("Without accounts, the state of a request is checked and its payload is not read",
          "[lobby][accounts]")
{
    LobbyFixture fixture;
    auto client = fixture.client();

    // Before HELLO
    client->send(message_type::SignIn, encode(SignInMessage{"Pavel", "correct horse battery"}));
    CHECK(client->error(fixture.loop, 0) == ErrorMessage{message_type::SignIn, ErrorCode::HelloRequired});

    // Once welcomed, a payload that would be malformed changes nothing: no account is behind it
    client->hello("Pavel");
    REQUIRE(client->reply(fixture.loop, 1));
    client->send(message_type::SignIn, Bytes{std::byte{0xFF}});
    client->send(message_type::SignOut, Bytes{std::byte{1}});
    CHECK(client->error(fixture.loop, 2) == ErrorMessage{message_type::SignIn, ErrorCode::AccountsDisabled});
    CHECK(client->error(fixture.loop, 3) == ErrorMessage{message_type::SignOut, ErrorCode::AccountsDisabled});
    CHECK_FALSE(client->closed);

    // A type after the accounts is still unknown
    client->send(0x14);
    CHECK(client->error(fixture.loop, 4) == ErrorMessage{0x14, ErrorCode::UnknownMessage});
}

TEST_CASE("A HELLO with a session token gets SESSION_EXPIRED; without it, the client is welcomed",
          "[lobby][accounts]")
{
    LobbyFixture fixture;
    auto resuming = fixture.client();

    resuming->send(
        message_type::Hello,
        encode(HelloMessage{.version = ProtocolVersion, .name = "", .locale = "en", .token = someToken()}));

    CHECK(resuming->error(fixture.loop) == ErrorMessage{message_type::Hello, ErrorCode::SessionExpired});
    CHECK(waitUntil(fixture.loop, [&] { return resuming->closed.has_value(); }));

    // The client forgets the token and connects again, as a guest
    auto guest = fixture.player("Pavel");
    CHECK_FALSE(decodeWelcome(guest->received[0].second)->account);
}

TEST_CASE("No password, token or code of the accounts reaches the logs", "[lobby][accounts]")
{
    const Captured captured;
    LobbyFixture fixture;
    const std::string password = "my very secret password";
    const std::string code = "864209";

    auto resuming = fixture.client();
    resuming->send(
        message_type::Hello,
        encode(HelloMessage{.version = ProtocolVersion, .name = "", .locale = "en", .token = someToken()}));
    REQUIRE(resuming->error(fixture.loop));

    auto client = fixture.player("Pavel");
    client->send(message_type::SignIn, encode(SignInMessage{"Pavel", password}));
    client->send(message_type::Register, encode(RegisterMessage{"Pavel", "pavel@example.org", password}));
    client->send(message_type::ResetPassword,
                 encode(ResetPasswordMessage{"pavel@example.org", code, password}));
    client->send(message_type::DeleteAccount, encode(DeleteAccountMessage{password}));
    client->send(message_type::VerifyEmail, encode(VerifyEmailMessage{"Pavel", code}));
    REQUIRE(client->reply(fixture.loop, 5));

    const std::string logs = captured.sink->text();
    CHECK_THAT(logs, ContainsSubstring("Pavel"));
    CHECK_THAT(logs, !ContainsSubstring(password));
    CHECK_THAT(logs, !ContainsSubstring(code));
    CHECK_THAT(logs, !ContainsSubstring(std::string(32, '\x5A')));
    CHECK_THAT(logs, !ContainsSubstring("5a5a5a5a"));
    CHECK_THAT(logs, !ContainsSubstring("5A5A5A5A"));
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
        // A second HELLO is ALREADY_WELCOMED and a LEAVE_GAME out of a game NOT_IN_GAME, whatever they hold:
        // the state is checked first
        const std::vector<std::pair<std::uint8_t, Bytes>> requests = {
            {message_type::Ping, Bytes{std::byte{0}}},
            {message_type::ListGames, Bytes{std::byte{0}}},
            {message_type::CreateGame, Bytes{std::byte{1}, std::byte{'A'}, std::byte{5}}},
            {message_type::JoinGame, Bytes{std::byte{1}}},
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
