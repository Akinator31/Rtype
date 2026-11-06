// Tests of the games of the master: real instances of r-type_server started by Games, real lobby clients over
// TLS, and real game clients over UDP (docs/protocol/lobby.md, docs/protocol/instances.md,
// docs/architecture/server.md).

#include "Common/Options.hpp"
#include "GameServers.hpp"
#include "Http.hpp"
#include "LobbyClient.hpp"
#include "Master/Games.hpp"
#include "Master/Lobby.hpp"
#include "Master/Process.hpp"
#include "Web/LocalAccess.hpp"
#include "rtype/game/Messages.hpp"
#include "rtype/lobby/Messages.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/Datagram.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/FramedConnection.hpp"
#include "rtype/network/Session.hpp"
#include "rtype/network/TcpConnection.hpp"
#include "rtype/network/TcpListener.hpp"
#include "rtype/network/Tls.hpp"
#include "rtype/network/UdpSocket.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace rtype::server;
using namespace rtype::lobby;
using namespace std::chrono_literals;
using rtype::network::Address;
using rtype::network::ClientSession;
using rtype::network::DatagramKey;
using rtype::network::DisconnectReason;
using rtype::network::EventLoop;
using rtype::network::ServerIdentity;
using rtype::network::SessionEnd;
using rtype::network::TcpListener;
using rtype::network::UdpSocket;
using testing::freePorts;
using testing::instances;
using testing::killProcess;
using testing::LobbyClient;
using testing::waitUntil;
using Clock = std::chrono::steady_clock;
using Bytes = std::vector<std::byte>;

namespace
{

/// A client of a game, over UDP, with the keys of its GAME_JOINED.
struct GameClient {
    UdpSocket socket;
    Address game;
    ClientSession session;
    std::optional<rtype::game::AcceptedMessage> accepted;
    /// The latest snapshot part received.
    std::optional<rtype::game::SnapshotPart> snapshot;
    std::optional<SessionEnd> end;
    bool updating = true;

    GameClient(EventLoop &loop, const GameJoinedMessage &joined)
        : socket(loop, Address::loopbackIpv4(0)), game(Address::loopbackIpv4(joined.udpPort)),
          session(joined.sessionId, DatagramKey(joined.clientKey), DatagramKey(joined.serverKey),
                  [this](std::span<const std::byte> datagram) { socket.send(game, datagram); })
    {
        socket.startReceiving([this](const Address &from, std::span<const std::byte> datagram) {
            if (from == game) {
                session.receive(datagram, Clock::now());
            }
        });
        session.start(
            [this](std::span<const std::byte> payload) { accepted = rtype::game::decodeAccepted(payload); },
            [this](std::uint8_t type, std::span<const std::byte> payload) {
                if (type == rtype::game::message_type::Snapshot) {
                    if (auto part = rtype::game::decodeSnapshotPart(payload)) {
                        snapshot = std::move(part);
                    }
                }
            },
            [this](SessionEnd ended) { end = ended; }, Clock::now());
    }

    void sendInput(std::uint32_t tick, rtype::game::PlayerInput input)
    {
        session.send(rtype::game::message_type::Input,
                     rtype::game::encode(rtype::game::InputMessage{tick, {input}}),
                     rtype::network::Delivery::Unreliable);
    }

    void update()
    {
        if (updating) {
            session.update(Clock::now());
        }
    }
};

/// A master: its games and its lobby, on the loop of the test.
struct MasterFixture {
    EventLoop loop;
    ServerIdentity identity = ServerIdentity::generate();
    Games games;
    Lobby lobby;
    std::vector<GameClient *> gameClients;

    explicit MasterFixture(GamesSettings settings = instances())
        : games(loop, std::move(settings)),
          lobby(loop, TcpListener(loop, Address::loopbackIpv4(0)), identity, games)
    {
    }

    ~MasterFixture()
    {
        // The instances left stop properly
        games.shutdown();
        (void)waitUntil(loop, [&] { return games.empty(); });
    }

    MasterFixture(const MasterFixture &) = delete;
    MasterFixture &operator=(const MasterFixture &) = delete;

    std::unique_ptr<LobbyClient> player(const std::string &name)
    {
        auto created = std::make_unique<LobbyClient>(loop, lobby.localAddress(), identity.fingerprint());
        REQUIRE(waitUntil(loop, [&] { return created->connection.has_value(); }));
        created->hello(name);
        REQUIRE(waitUntil(loop, [&] { return !created->received.empty(); }));
        REQUIRE(created->received[0].first == message_type::Welcome);
        return created;
    }

    /// Waits, updating the game clients, until the condition holds.
    bool wait(const std::function<bool()> &condition)
    {
        return waitUntil(loop, [&] {
            for (GameClient *client : gameClients) {
                client->update();
            }
            return condition();
        });
    }

    /// The reply number `index` of a client, which must be GAME_JOINED.
    GameJoinedMessage joined(LobbyClient &client, std::size_t index)
    {
        REQUIRE(wait([&] { return client.received.size() > index; }));
        REQUIRE(client.received[index].first == message_type::GameJoined);
        return *decodeGameJoined(client.received[index].second);
    }

    GameListMessage list(LobbyClient &client)
    {
        const std::size_t index = client.received.size();
        client.send(message_type::ListGames);
        REQUIRE(wait([&] { return client.received.size() > index; }));
        REQUIRE(client.received[index].first == message_type::GameList);
        return *decodeGameList(client.received[index].second);
    }
};

} // namespace

TEST_CASE("Secrets are compared byte for byte", "[games]")
{
    Secret first{};
    Secret second{};
    CHECK(sameSecret(first, second));
    second[31] = std::byte{1};
    CHECK_FALSE(sameSecret(first, second));
    second[31] = std::byte{0};
    second[0] = std::byte{0x80};
    CHECK_FALSE(sameSecret(first, second));
}

TEST_CASE("A player creates a game, others list it and join it, and it ends with its last player", "[games]")
{
    MasterFixture master;
    auto alice = master.player("Alice");
    auto bob = master.player("Bob");
    auto carol = master.player("Carol");

    // Alice creates it: answered once its instance is ready
    alice->send(message_type::CreateGame, encode(CreateGameMessage{"Bydo hunters", 2}));
    const GameJoinedMessage aliceGame = master.joined(*alice, 1);
    CHECK(aliceGame.gameId != 0);
    CHECK(aliceGame.sessionId != 0);
    GameClient aliceClient(master.loop, aliceGame);
    master.gameClients.push_back(&aliceClient);
    REQUIRE(master.wait([&] { return aliceClient.accepted.has_value(); }));
    CHECK(aliceClient.accepted->slot == 0);

    // Bob sees it, and joins it
    CHECK(master.list(*bob) == GameListMessage{{{aliceGame.gameId, "Bydo hunters", 1, 2}}});
    bob->send(message_type::JoinGame, encode(JoinGameMessage{aliceGame.gameId}));
    const GameJoinedMessage bobGame = master.joined(*bob, 2);
    CHECK(bobGame.udpPort == aliceGame.udpPort);
    CHECK(bobGame.sessionId != aliceGame.sessionId);
    CHECK(bobGame.clientKey != aliceGame.clientKey);
    GameClient bobClient(master.loop, bobGame);
    master.gameClients.push_back(&bobClient);
    REQUIRE(master.wait([&] { return bobClient.accepted.has_value(); }));
    CHECK(bobClient.accepted->slot == 1);

    // Full, and a game that does not exist
    carol->send(message_type::JoinGame, encode(JoinGameMessage{aliceGame.gameId}));
    carol->send(message_type::JoinGame, encode(JoinGameMessage{999}));
    CHECK(carol->error(master.loop, 1) == ErrorMessage{message_type::JoinGame, ErrorCode::GameFull});
    CHECK(carol->error(master.loop, 2) == ErrorMessage{message_type::JoinGame, ErrorCode::GameNotFound});
    // In a game, no other one
    bob->send(message_type::CreateGame, encode(CreateGameMessage{"Another", 1}));
    CHECK(bob->error(master.loop, 3) == ErrorMessage{message_type::CreateGame, ErrorCode::AlreadyInGame});

    // Bob leaves: answered, and its session closed by the game
    bob->send(message_type::LeaveGame);
    REQUIRE(master.wait([&] { return bob->received.size() > 4 && bobClient.end.has_value(); }));
    CHECK(decodeLeftGame(bob->received[4].second) ==
          LeftGameMessage{aliceGame.gameId, LeftGameReason::Requested});
    CHECK(bobClient.end->reason == DisconnectReason::Quit);
    CHECK(master.list(*carol).games[0].players == 1);

    // Alice's lobby connection closes: she is removed from the game (REMOVE_PLAYER, so DISCONNECT (QUIT)),
    // which has no player left, and its instance stops
    alice.reset();
    REQUIRE(master.wait([&] { return aliceClient.end.has_value(); }));
    CHECK(aliceClient.end->reason == DisconnectReason::Quit);
    REQUIRE(master.wait([&] { return master.games.empty(); }));
    CHECK(master.list(*carol).games.empty());
}

TEST_CASE("The requests after CREATE_GAME are answered after it, in order", "[games]")
{
    MasterFixture master;
    auto alice = master.player("Alice");

    alice->send(message_type::CreateGame, encode(CreateGameMessage{"Solo", 1}));
    alice->send(message_type::Ping);
    alice->send(message_type::ListGames);
    alice->send(message_type::JoinGame, encode(JoinGameMessage{1}));

    REQUIRE(master.wait([&] { return alice->received.size() >= 5; }));
    CHECK(alice->received[1].first == message_type::GameJoined);
    CHECK(alice->received[2].first == message_type::Pong);
    REQUIRE(alice->received[3].first == message_type::GameList);
    CHECK(decodeGameList(alice->received[3].second)->games.size() == 1);
    CHECK(decodeError(alice->received[4].second) ==
          ErrorMessage{message_type::JoinGame, ErrorCode::AlreadyInGame});
}

TEST_CASE("Session ids are unique across the games of the master, and drawn again on a collision", "[games]")
{
    // The draws, in order; 0 and the ids already taken are drawn again:
    // - Alice, game 1: 0 refused, then 7
    // - Bob, game 2: 7 taken in game 1, then 9
    // - Carol, game 1: 9 taken in game 2, 7 in game 1, then 12
    // - Dave, game 1 after Carol left: 12 still taken until the instance exits, then 13
    auto draws = std::make_shared<std::vector<std::uint32_t>>(
        std::vector<std::uint32_t>{0, 7, 7, 9, 9, 7, 12, 12, 13});
    MasterFixture master;
    master.games.setSessionDraw([draws, extra = std::uint32_t{1000}]() mutable {
        if (draws->empty()) {
            return extra++; // more draws than expected: the checks below fail, without looping forever
        }
        const std::uint32_t session = draws->front();
        draws->erase(draws->begin());
        return session;
    });
    auto alice = master.player("Alice");
    auto bob = master.player("Bob");
    auto carol = master.player("Carol");
    auto dave = master.player("Dave");

    alice->send(message_type::CreateGame, encode(CreateGameMessage{"First", 4}));
    const GameJoinedMessage aliceGame = master.joined(*alice, 1);
    bob->send(message_type::CreateGame, encode(CreateGameMessage{"Second", 4}));
    const GameJoinedMessage bobGame = master.joined(*bob, 1);
    carol->send(message_type::JoinGame, encode(JoinGameMessage{aliceGame.gameId}));
    const GameJoinedMessage carolGame = master.joined(*carol, 1);
    CHECK(aliceGame.sessionId == 7);
    CHECK(bobGame.sessionId == 9);
    CHECK(carolGame.sessionId == 12);
    CHECK(bobGame.gameId != aliceGame.gameId);

    // The instances accept these ids: the clients of both games connect
    GameClient aliceClient(master.loop, aliceGame);
    GameClient bobClient(master.loop, bobGame);
    GameClient carolClient(master.loop, carolGame);
    master.gameClients = {&aliceClient, &bobClient, &carolClient};
    REQUIRE(master.wait([&] {
        return aliceClient.accepted.has_value() && bobClient.accepted.has_value() &&
               carolClient.accepted.has_value();
    }));

    carol->send(message_type::LeaveGame);
    REQUIRE(master.wait([&] { return carol->received.size() > 2 && carolClient.end.has_value(); }));
    dave->send(message_type::JoinGame, encode(JoinGameMessage{aliceGame.gameId}));
    const GameJoinedMessage daveGame = master.joined(*dave, 1);
    CHECK(daveGame.sessionId == 13);
    CHECK(draws->empty());
}

TEST_CASE("Without a free game port, the server is full", "[games]")
{
    MasterFixture master(instances(1));
    auto alice = master.player("Alice");
    auto bob = master.player("Bob");

    alice->send(message_type::CreateGame, encode(CreateGameMessage{"First", 4}));
    (void)master.joined(*alice, 1);
    bob->send(message_type::CreateGame, encode(CreateGameMessage{"Second", 4}));

    CHECK(bob->error(master.loop, 1) == ErrorMessage{message_type::CreateGame, ErrorCode::ServerFull});
}

TEST_CASE("A game whose instance does not start fails, and its port is free again", "[games]")
{
    SECTION("A program that does not exist")
    {
        GamesSettings settings = instances(1);
        settings.executable = std::filesystem::path(RTYPE_TEST_SERVER).parent_path() / "no_such_program";
        MasterFixture master(settings);
        auto alice = master.player("Alice");

        alice->send(message_type::CreateGame, encode(CreateGameMessage{"Game", 4}));

        CHECK(alice->error(master.loop, 1) ==
              ErrorMessage{message_type::CreateGame, ErrorCode::GameStartFailed});
    }
    SECTION("An instance that exits at once")
    {
        GamesSettings settings = instances(1);
        // The helper refuses --instance
        settings.executable = RTYPE_TEST_CHILD;
        MasterFixture master(settings);
        auto alice = master.player("Alice");

        alice->send(message_type::CreateGame, encode(CreateGameMessage{"Game", 4}));
        CHECK(alice->error(master.loop, 1) ==
              ErrorMessage{message_type::CreateGame, ErrorCode::GameStartFailed});

        // Not SERVER_FULL: the only port was given back
        REQUIRE(master.wait([&] { return master.games.empty(); }));
        alice->send(message_type::CreateGame, encode(CreateGameMessage{"Game", 4}));
        CHECK(alice->error(master.loop, 2) ==
              ErrorMessage{message_type::CreateGame, ErrorCode::GameStartFailed});
    }
    SECTION("An instance that never gets ready")
    {
        GamesSettings settings = instances(1);
        settings.executable = RTYPE_TEST_CHILD;
        settings.leadingArguments = {"sleep"};
        settings.startTimeout = 300ms;
        MasterFixture master(settings);
        auto alice = master.player("Alice");
        const auto start = Clock::now();

        alice->send(message_type::CreateGame, encode(CreateGameMessage{"Game", 4}));

        CHECK(alice->error(master.loop, 1) ==
              ErrorMessage{message_type::CreateGame, ErrorCode::GameStartFailed});
        CHECK(Clock::now() - start >= 250ms);
        // Killed, and forgotten
        CHECK(master.wait([&] { return master.games.empty(); }));
    }
}

TEST_CASE("Only the instance a game started can present itself, with its secret", "[games]")
{
    GamesSettings settings = instances(1);
    // An instance that never connects: the test presents itself in its place
    settings.executable = RTYPE_TEST_CHILD;
    settings.leadingArguments = {"sleep"};
    MasterFixture master(settings);
    auto alice = master.player("Alice");
    alice->send(message_type::CreateGame, encode(CreateGameMessage{"Game", 4}));
    REQUIRE(master.wait([&] { return master.games.count() == 1; }));

    const auto present = [&](std::uint32_t id) {
        std::optional<rtype::network::FramedConnection> impostor;
        bool closed = false;
        rtype::network::TcpConnection::connect(
            master.loop, Address::loopbackIpv4(master.games.instancePort()),
            [&](std::error_code, std::optional<rtype::network::TcpConnection> tcp) {
                impostor.emplace(std::move(*tcp));
                impostor->start([](std::uint8_t, std::span<const std::byte>) {},
                                [&](std::error_code) { closed = true; });
                impostor->send(instance_message::InstanceHello,
                               encode(InstanceHelloMessage{.id = id, .secret = {}}));
            });
        return master.wait([&] { return closed; });
    };

    // An unknown id: closed, and the game goes on starting
    CHECK(present(999));
    CHECK(alice->received.size() == 1);
    // The right id with a wrong secret: closed, and the game fails at once
    CHECK(present(1));
    CHECK(alice->error(master.loop, 1) == ErrorMessage{message_type::CreateGame, ErrorCode::GameStartFailed});
}

TEST_CASE("An instance that crashes is reported to its players", "[games]")
{
    MasterFixture master;
    auto alice = master.player("Alice");
    alice->send(message_type::CreateGame, encode(CreateGameMessage{"Fragile", 2}));
    const GameJoinedMessage game = master.joined(*alice, 1);
    const std::optional<int> pid = master.games.processId(game.gameId);
    REQUIRE(pid);

    killProcess(*pid);

    REQUIRE(master.wait([&] { return alice->received.size() > 2; }));
    CHECK(alice->received[2].first == message_type::LeftGame);
    CHECK(decodeLeftGame(alice->received[2].second) ==
          LeftGameMessage{game.gameId, LeftGameReason::GameCrashed});
    REQUIRE(master.wait([&] { return master.games.empty(); }));
    // Back in the lobby: Alice can create another one
    alice->send(message_type::CreateGame, encode(CreateGameMessage{"Sturdy", 2}));
    (void)master.joined(*alice, 3);
}

TEST_CASE("A player whose client goes silent is told it timed out", "[games]")
{
    MasterFixture master;
    auto alice = master.player("Alice");
    alice->send(message_type::CreateGame, encode(CreateGameMessage{"Silent", 2}));
    GameClient client(master.loop, master.joined(*alice, 1));
    master.gameClients.push_back(&client);
    REQUIRE(master.wait([&] { return client.accepted.has_value(); }));

    // No more datagrams from it: the game closes the session after 5 seconds
    client.updating = false;

    REQUIRE(master.wait([&] { return alice->received.size() > 2; }));
    CHECK(decodeLeftGame(alice->received[2].second)->reason == LeftGameReason::TimedOut);
    CHECK(master.wait([&] { return master.games.empty(); }));
}

TEST_CASE("The master reads the score of a player who just destroyed a Bydo, within two seconds", "[games]")
{
    MasterFixture master;
    auto alice = master.player("Alice");
    alice->send(message_type::CreateGame, encode(CreateGameMessage{"Scores", 1}));
    const GameJoinedMessage joined = master.joined(*alice, 1);
    GameClient client(master.loop, joined);
    master.gameClients.push_back(&client);
    REQUIRE(master.wait([&] { return client.accepted && client.snapshot; }));

    // Like a player: one INPUT per tick, firing, the ship following the nearest Bydo up and down
    const auto scoreOf = [](const rtype::game::SnapshotPart &part) {
        return part.players.empty() ? 0U : part.players[0].score;
    };
    std::uint32_t stamped = 0;
    std::optional<float> target;
    auto nextInput = Clock::now();
    const auto play = [&] {
        client.update();
        const rtype::game::SnapshotPart &part = *client.snapshot;
        std::optional<rtype::game::EntityState> ship;
        target.reset();
        float nearest = 1e9F;
        for (const rtype::game::EntityState &entity : part.entities) {
            if (entity.kind == rtype::game::EntityKind::PlayerShip) {
                ship = entity;
            }
        }
        for (const rtype::game::EntityState &entity : part.entities) {
            const auto kind = static_cast<std::uint8_t>(entity.kind);
            if (ship && kind >= 0x10 && kind < 0x40 && entity.x > ship->x && entity.x - ship->x < nearest) {
                nearest = entity.x - ship->x;
                target = entity.y;
            }
        }
        if (Clock::now() < nextInput) {
            return;
        }
        nextInput += std::chrono::nanoseconds(std::chrono::seconds(1)) / 60;
        rtype::game::PlayerInput input{rtype::game::PlayerInput::Fire};
        if (ship && target && *target < ship->y - 8.0F) {
            input.set(rtype::game::PlayerInput::Up, true);
        } else if (ship && target && *target > ship->y + 8.0F) {
            input.set(rtype::game::PlayerInput::Down, true);
        }
        stamped = std::max(stamped + 1, part.tick + 5);
        client.sendInput(stamped, input);
    };
    REQUIRE(testing::waitUntil(
        master.loop,
        [&] {
            play();
            return scoreOf(*client.snapshot) > 0;
        },
        60s));
    const std::uint32_t score = scoreOf(*client.snapshot);

    // Within two seconds, the master knows it from the GAME_STATUS of the instance
    const auto destroyed = Clock::now();
    std::optional<GameStatus> status;
    REQUIRE(testing::waitUntil(
        master.loop,
        [&] {
            client.update();
            status = master.games.status(joined.gameId);
            return status && status->last && status->last->players.size() == 1 &&
                   status->last->players[0].score >= score;
        },
        2s));
    CHECK(Clock::now() - destroyed <= 2s);
    CHECK(status->last->players[0].playerId == decodeWelcome(alice->received[0].second)->playerId);
    CHECK(status->last->wave >= 1);
    CHECK(status->last->players[0].latency);
    CHECK_FALSE(status->stale);
}

TEST_CASE("A game whose instance stops reporting is shown as stale", "[games]")
{
    const std::filesystem::path secretFile = std::filesystem::temp_directory_path() /
                                             ("rtype_tests_stale_" + std::to_string(std::random_device{}()));
    GamesSettings settings = instances(1);
    // An instance that gives its secret to the test, which plays it
    settings.executable = RTYPE_TEST_CHILD;
    settings.leadingArguments = {"relay", secretFile.string()};
    settings.statusTimeout = 500ms;
    MasterFixture master(settings);
    auto alice = master.player("Alice");
    alice->send(message_type::CreateGame, encode(CreateGameMessage{"Stuck", 2}));
    REQUIRE(master.wait([&] { return std::filesystem::exists(secretFile); }));
    std::string line;
    std::getline(std::ifstream(secretFile), line);
    std::filesystem::remove(secretFile);
    const std::optional<Secret> secret = parseSecret(line);
    REQUIRE(secret);

    std::optional<rtype::network::FramedConnection> instance;
    std::vector<std::uint8_t> received;
    rtype::network::TcpConnection::connect(
        master.loop, Address::loopbackIpv4(master.games.instancePort()),
        [&](std::error_code, std::optional<rtype::network::TcpConnection> tcp) {
            instance.emplace(std::move(*tcp));
            instance->start([&](std::uint8_t type, std::span<const std::byte>) { received.push_back(type); },
                            [](std::error_code) {});
            instance->send(instance_message::InstanceHello,
                           encode(InstanceHelloMessage{.id = 1, .secret = *secret}));
        });
    REQUIRE(master.wait([&] { return !received.empty(); }));
    REQUIRE(received[0] == instance_message::Configure);
    CHECK_FALSE(master.games.status(1));
    instance->send(instance_message::Ready, encode(ReadyMessage{settings.firstPort}));
    const GameJoinedMessage joined = master.joined(*alice, 1);

    // Ready, nothing reported yet: no status, not stale yet
    std::optional<GameStatus> status = master.games.status(joined.gameId);
    REQUIRE(status);
    CHECK_FALSE(status->last);
    CHECK_FALSE(status->stale);

    const std::uint32_t aliceId = decodeWelcome(alice->received[0].second)->playerId;
    const GameStatusMessage reported{600, 1, {{aliceId, 300, true, 12}}};
    instance->send(instance_message::GameStatus, encode(reported));
    REQUIRE(master.wait([&] {
        status = master.games.status(joined.gameId);
        return status && status->last;
    }));
    CHECK(status->last == reported);
    CHECK_FALSE(status->stale);
    CHECK(status->age < 500ms);

    // Nothing more: past the timeout, the last status stays, marked stale
    REQUIRE(master.wait([&] {
        status = master.games.status(joined.gameId);
        return status && status->stale;
    }));
    CHECK(status->last == reported);
    CHECK(status->age > 500ms);

    // A malformed one fails the game, as any malformed message of an instance
    instance->send(instance_message::GameStatus, Bytes{std::byte{1}});
    REQUIRE(master.wait([&] { return alice->received.size() > 2; }));
    CHECK(decodeLeftGame(alice->received[2].second)->reason == LeftGameReason::GameCrashed);
    CHECK_FALSE(master.games.status(joined.gameId));
}

TEST_CASE("A master that stops shuts its games down", "[games]")
{
    MasterFixture master;
    auto alice = master.player("Alice");
    alice->send(message_type::CreateGame, encode(CreateGameMessage{"Game", 2}));
    GameClient client(master.loop, master.joined(*alice, 1));
    master.gameClients.push_back(&client);
    REQUIRE(master.wait([&] { return client.accepted.has_value(); }));

    master.lobby.close();
    master.games.shutdown();

    REQUIRE(master.wait([&] { return client.end.has_value(); }));
    CHECK(client.end->reason == DisconnectReason::Shutdown);
    CHECK(master.wait([&] { return master.games.empty(); }));
    CHECK(master.wait([&] { return alice->closed.has_value(); }));
}

TEST_CASE("r-type_server, run as a master, starts the instance of a game created by a client", "[games]")
{
    EventLoop loop;
    const ServerIdentity identity = ServerIdentity::generate();
    const std::filesystem::path key = std::filesystem::temp_directory_path() / "rtype_tests_games_master.key";
    std::ofstream(key) << identity.privateKeyPem();
    const std::filesystem::path database =
        std::filesystem::temp_directory_path() / "rtype_tests_games_master.db";
    std::filesystem::remove(database);
    const std::filesystem::path settings =
        testing::webOnAnyPort(std::filesystem::temp_directory_path() / "rtype_tests_games_master.toml");
    std::uint16_t lobbyPort = 0;
    {
        const TcpListener probe(loop, Address::loopbackIpv4(0));
        lobbyPort = probe.localAddress().port();
    }
    loop.poll();
    const std::uint16_t gamePort = freePorts(1);
    const std::vector<std::string> arguments = {
        "--port",       std::to_string(lobbyPort),
        "--bind",       "127.0.0.1",
        "--game-ports", std::to_string(gamePort) + "-" + std::to_string(gamePort),
        "--key",        key.string(),
        "--database",   "sqlite:" + database.string(),
        "--settings",   settings.string(),
        "--log-level",  "warning"};
    Process server(RTYPE_TEST_SERVER, arguments, {.input = {}, .stopGrace = testing::Patience});

    // The master opens its port: try until it does
    std::unique_ptr<LobbyClient> alice;
    REQUIRE(waitUntil(loop, [&] {
        if (!alice || alice->closed) {
            alice =
                std::make_unique<LobbyClient>(loop, Address::loopbackIpv4(lobbyPort), identity.fingerprint());
            testing::runFor(loop, 50ms);
        }
        return alice->secured;
    }));
    alice->hello("Alice");
    alice->send(message_type::CreateGame, encode(CreateGameMessage{"For real", 2}));
    REQUIRE(waitUntil(loop, [&] { return alice->received.size() >= 2; }));
    REQUIRE(alice->received[1].first == message_type::GameJoined);
    const GameJoinedMessage game = *decodeGameJoined(alice->received[1].second);
    CHECK(game.udpPort == gamePort);
    GameClient client(loop, game);
    REQUIRE(waitUntil(loop, [&] {
        client.update();
        return client.accepted.has_value();
    }));

    // Ctrl+C, or SIGTERM: the game is shut down, then the master exits properly
    server.terminate();
    REQUIRE(waitUntil(loop, [&] {
        client.update();
        return client.end.has_value();
    }));
    CHECK(client.end->reason == DisconnectReason::Shutdown);
    CHECK(server.wait(testing::Patience) == Success);
    std::filesystem::remove(key);
    std::filesystem::remove(database);
    std::filesystem::remove(settings);
    std::filesystem::remove(std::filesystem::temp_directory_path() / AdminUrlFileName);
}
