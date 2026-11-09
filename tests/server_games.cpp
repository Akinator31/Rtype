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
#include "Master/Relay.hpp"
#include "Master/VoiceMutes.hpp"
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
    /// The latest VOICE_STATUSES received.
    std::optional<rtype::game::VoiceStatusesMessage> statuses;
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
                } else if (type == rtype::game::message_type::VoiceStatuses) {
                    statuses = rtype::game::decodeVoiceStatuses(payload);
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

/// A master: its relay, its games and its lobby, on the loop of the test.
struct MasterFixture {
    EventLoop loop;
    ServerIdentity identity = ServerIdentity::generate();
    /// Before the games, which it outlives
    Relay relay{Address::loopbackIpv4(0)};
    Games games;
    Lobby lobby;
    std::vector<GameClient *> gameClients;

    explicit MasterFixture(GamesSettings settings = instances())
        : games(loop, relay, std::move(settings)),
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

TEST_CASE("With max_games games running, the server is full", "[games]")
{
    MasterFixture master(instances(1));
    auto alice = master.player("Alice");
    auto bob = master.player("Bob");

    alice->send(message_type::CreateGame, encode(CreateGameMessage{"First", 4}));
    (void)master.joined(*alice, 1);
    bob->send(message_type::CreateGame, encode(CreateGameMessage{"Second", 4}));

    CHECK(bob->error(master.loop, 1) == ErrorMessage{message_type::CreateGame, ErrorCode::ServerFull});
}

TEST_CASE("A game whose instance does not start fails, and its place is free again", "[games]")
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

        // Not SERVER_FULL: the only place was given back
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
    auto nextPing = Clock::now();
    const auto play = [&] {
        client.update();
        // The lobby closes a player silent for 30 s, and its game with it: on a slow machine, the first kill
        // can take longer
        if (Clock::now() >= nextPing) {
            alice->send(message_type::Ping);
            nextPing += 5s;
        }
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
    // Where it would receive the frames of the relay: no client plays here
    instance->send(instance_message::Ready, encode(ReadyMessage{1}));
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

TEST_CASE("Every game is served on the one UDP port of the relay, each player seeing only its game",
          "[games][relay]")
{
    // Declared before the clients, which it outlives
    MasterFixture master(instances(2));
    Relay &relay = master.relay;
    const std::uint16_t relayPort = relay.publicAddress().port();
    auto alice = master.player("Alice");
    auto bob = master.player("Bob");
    auto carol = master.player("Carol");
    auto dave = master.player("Dave");

    // Two games of two players
    alice->send(message_type::CreateGame, encode(CreateGameMessage{"First", 2}));
    const GameJoinedMessage aliceGame = master.joined(*alice, 1);
    carol->send(message_type::CreateGame, encode(CreateGameMessage{"Second", 2}));
    const GameJoinedMessage carolGame = master.joined(*carol, 1);
    bob->send(message_type::JoinGame, encode(JoinGameMessage{aliceGame.gameId}));
    const GameJoinedMessage bobGame = master.joined(*bob, 1);
    dave->send(message_type::JoinGame, encode(JoinGameMessage{carolGame.gameId}));
    const GameJoinedMessage daveGame = master.joined(*dave, 1);
    for (const GameJoinedMessage *joined : {&aliceGame, &bobGame, &carolGame, &daveGame}) {
        CHECK(joined->udpPort == relayPort);
    }
    CHECK(carolGame.gameId != aliceGame.gameId);

    GameClient aliceClient(master.loop, aliceGame);
    GameClient bobClient(master.loop, bobGame);
    GameClient carolClient(master.loop, carolGame);
    GameClient daveClient(master.loop, daveGame);
    master.gameClients = {&aliceClient, &bobClient, &carolClient, &daveClient};
    // Two players in each snapshot: its own game, never the other
    const auto bothIn = [](const GameClient &client) {
        return client.accepted && client.snapshot && client.snapshot->players.size() == 2;
    };
    REQUIRE(master.wait([&] {
        return bothIn(aliceClient) && bothIn(bobClient) && bothIn(carolClient) && bothIn(daveClient);
    }));
    CHECK(aliceClient.accepted->slot != bobClient.accepted->slot);
    CHECK(carolClient.accepted->slot != daveClient.accepted->slot);
    CHECK(relay.counters().inbound > 0);
    CHECK(relay.counters().outbound > 0);
    CHECK(relay.counters().droppedFor(Relay::Drop::WrongInstance) == 0);
    CHECK(relay.counters().droppedFor(Relay::Drop::UnknownSession) == 0);

    // Bob leaves: the DISCONNECT (QUIT) of its instance still goes out through the relay
    bob->send(message_type::LeaveGame);
    REQUIRE(master.wait([&] { return bobClient.end.has_value(); }));
    CHECK(bobClient.end->reason == DisconnectReason::Quit);

    // The instance of the second game crashes: its players are told, and its sessions are gone from the relay
    const std::optional<int> pid = master.games.processId(carolGame.gameId);
    REQUIRE(pid);
    killProcess(*pid);
    REQUIRE(master.wait([&] { return carol->received.size() > 2 && dave->received.size() > 2; }));
    CHECK(decodeLeftGame(carol->received[2].second)->reason == LeftGameReason::GameCrashed);
    REQUIRE(master.wait([&] { return master.games.count() == 1; }));
    // The heartbeats of its clients no longer reach any game
    const std::uint64_t unknown = relay.counters().droppedFor(Relay::Drop::UnknownSession);
    REQUIRE(
        master.wait([&] { return relay.counters().droppedFor(Relay::Drop::UnknownSession) >= unknown + 2; }));
    // The first game goes on
    CHECK_FALSE(aliceClient.end);
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
        testing::onAnyPorts(std::filesystem::temp_directory_path() / "rtype_tests_games_master.toml");
    // One number for the lobby and the games, as 4242 by default: free in TCP and in UDP
    std::uint16_t port = 0;
    for (int attempt = 0; attempt < 100 && port == 0; ++attempt) {
        const TcpListener probe(loop, Address::loopbackIpv4(0));
        try {
            const UdpSocket udp(loop, Address::anyIpv4(probe.localAddress().port()));
            port = probe.localAddress().port();
        } catch (const std::system_error &) {
            continue;
        }
    }
    REQUIRE(port != 0);
    loop.poll();
    const std::vector<std::string> arguments = {"--port",      std::to_string(port),
                                                "--bind",      "127.0.0.1",
                                                "--game-port", std::to_string(port),
                                                "--key",       key.string(),
                                                "--database",  "sqlite:" + database.string(),
                                                "--settings",  settings.string(),
                                                "--log-level", "warning"};
    Process server(RTYPE_TEST_SERVER, arguments, {.input = {}, .stopGrace = testing::Patience});

    // The master opens its port: try until it does
    std::unique_ptr<LobbyClient> alice;
    REQUIRE(waitUntil(loop, [&] {
        if (!alice || alice->closed) {
            alice = std::make_unique<LobbyClient>(loop, Address::loopbackIpv4(port), identity.fingerprint());
            testing::runFor(loop, 50ms);
        }
        return alice->secured;
    }));
    alice->hello("Alice");
    alice->send(message_type::CreateGame, encode(CreateGameMessage{"For real", 2}));
    REQUIRE(waitUntil(loop, [&] { return alice->received.size() >= 2; }));
    REQUIRE(alice->received[1].first == message_type::GameJoined);
    const GameJoinedMessage game = *decodeGameJoined(alice->received[1].second);
    CHECK(game.udpPort == port);
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

TEST_CASE("A game carries the voice as the settings say, and drops the voice of a player muted by an "
          "administrator, or by its account",
          "[games][voice]")
{
    const auto now = std::chrono::system_clock::now();
    VoiceMutes mutes(
        {VoiceMute{.accountId = 7, .createdAt = now, .expiresAt = now + 1h, .reason = "", .actor = "local"}});
    MasterFixture master;
    master.games.setVoiceMutes(&mutes);
    std::optional<Games::JoinResult> created;
    master.games.create({.id = 1, .name = "Alice", .accountId = 0}, CreateGameMessage{"Voices", 4},
                        [&](Games::JoinResult result) { created = std::move(result); });
    REQUIRE(master.wait([&] { return created.has_value(); }));
    REQUIRE(std::holds_alternative<GameJoinedMessage>(*created));
    const GameJoinedMessage aliceJoined = std::get<GameJoinedMessage>(*created);
    GameClient alice(master.loop, aliceJoined);
    master.gameClients.push_back(&alice);
    REQUIRE(master.wait([&] { return alice.accepted.has_value(); }));
    CHECK(alice.accepted->voice);

    // Bob plays with the account whose voice is muted: it joins muted
    const Games::JoinResult bobJoined =
        master.games.join({.id = 2, .name = "Bob", .accountId = 7}, aliceJoined.gameId);
    REQUIRE(std::holds_alternative<GameJoinedMessage>(bobJoined));
    GameClient bob(master.loop, std::get<GameJoinedMessage>(bobJoined));
    master.gameClients.push_back(&bob);
    const auto mutedByAdmin = [&](std::uint8_t slot) {
        return alice.statuses && alice.statuses->statuses[slot].mutedByAdmin;
    };
    REQUIRE(master.wait([&] { return bob.accepted.has_value() && mutedByAdmin(1); }));
    const auto shown = [&](std::uint32_t player) {
        const std::vector<AdminGame> games = master.games.adminGames();
        REQUIRE(games.size() == 1);
        const auto found = std::ranges::find(games[0].players, player, &AdminGamePlayer::id);
        REQUIRE(found != games[0].players.end());
        return *found;
    };
    CHECK(shown(2).voiceMutedUntil == now + 1h);
    CHECK_FALSE(shown(2).voiceMuted);

    // An administrator mutes Alice for the rest of the game
    CHECK(master.games.muteVoice(1, true));
    REQUIRE(master.wait([&] { return mutedByAdmin(0); }));
    CHECK(shown(1).voiceMuted);
    CHECK_FALSE(shown(1).voiceMutedUntil);

    // The mute of the account of Bob lifted: it talks again at once
    mutes.remove(7);
    CHECK(master.games.accountVoiceChanged(7) == 1);
    CHECK(master.games.accountVoiceChanged(8) == 0);
    REQUIRE(master.wait([&] { return !mutedByAdmin(1); }));
    CHECK(master.games.muteVoice(1, false));
    REQUIRE(master.wait([&] { return !mutedByAdmin(0); }));
    CHECK_FALSE(master.games.muteVoice(99, true));
    master.games.setVoiceMutes(nullptr);
}

TEST_CASE("A game of a master whose voice is off tells its clients so", "[games][voice]")
{
    GamesSettings settings = instances();
    settings.voice.enabled = false;
    MasterFixture master(settings);
    std::optional<Games::JoinResult> created;
    master.games.create({.id = 1, .name = "Alice", .accountId = 0}, CreateGameMessage{"Quiet", 2},
                        [&](Games::JoinResult result) { created = std::move(result); });
    REQUIRE(master.wait([&] { return created.has_value(); }));
    REQUIRE(std::holds_alternative<GameJoinedMessage>(*created));
    GameClient alice(master.loop, std::get<GameJoinedMessage>(*created));
    master.gameClients.push_back(&alice);
    REQUIRE(master.wait([&] { return alice.accepted.has_value(); }));
    CHECK_FALSE(alice.accepted->voice);
    CHECK_FALSE(master.games.adminGames().at(0).voice);
}
