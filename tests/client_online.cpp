// Tests of the client online: its LobbyClient and its RemoteSession against a real master (the Lobby and the
// Games of r-type_server, with real instances), all on the loop of the test
// (docs/architecture/client-online.md).

#include "Game/RemoteSession.hpp"
#include "GameServers.hpp"
#include "LobbyClient.hpp"
#include "Master/Games.hpp"
#include "Master/Lobby.hpp"
#include "Online/LobbyClient.hpp"
#include "Online/OnlineScenes.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/TcpListener.hpp"
#include "rtype/network/Tls.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

using namespace std::chrono_literals;
using rtype::client::LobbyClient;
using rtype::client::RemoteSession;
using rtype::network::Address;
using rtype::network::EventLoop;
using rtype::network::ServerIdentity;
using rtype::network::TcpListener;
using testing::waitUntil;

namespace lobby = rtype::lobby;
namespace game = rtype::game;

namespace
{

/// A master on the loopback: its games, run by real instances, and its lobby.
struct Master {
    EventLoop loop;
    ServerIdentity identity = ServerIdentity::generate();
    rtype::server::Games games{loop, testing::instances()};
    rtype::server::Lobby lobby{loop, TcpListener(loop, Address::loopbackIpv4(0)), identity, games};

    ~Master()
    {
        games.shutdown();
        (void)waitUntil(loop, [&] { return games.empty(); });
    }
};

/// What a LobbyClient told.
struct Events {
    std::optional<std::uint32_t> welcomed;
    std::vector<lobby::LeftGameMessage> left;
    std::optional<std::string> closed;

    LobbyClient::Handlers handlers()
    {
        return {
            .onWelcome = [this](std::uint32_t player) { welcomed = player; },
            .onLeftGame = [this](const lobby::LeftGameMessage &message) { left.push_back(message); },
            .onClosed = [this](const std::string &reason) { closed = reason; },
            .onUntrusted = {},
        };
    }
};

/// A request answered, waited for.
template <typename Result>
std::optional<Result> answer(EventLoop &loop,
                             const std::function<void(std::function<void(const Result &)>)> &send)
{
    std::optional<Result> result;
    send([&](const Result &value) { result = value; });
    if (!waitUntil(loop, [&] { return result.has_value(); })) {
        return std::nullopt;
    }
    return result;
}

lobby::GameJoinedMessage create(EventLoop &loop, LobbyClient &client, const std::string &name)
{
    const auto result = answer<LobbyClient::JoinResult>(
        loop, [&](auto done) { client.createGame(name, 2, std::move(done)); });
    REQUIRE(result);
    REQUIRE(std::holds_alternative<lobby::GameJoinedMessage>(*result));
    return std::get<lobby::GameJoinedMessage>(*result);
}

std::vector<lobby::GameInfo> list(EventLoop &loop, LobbyClient &client)
{
    const auto games =
        answer<std::vector<lobby::GameInfo>>(loop, [&](auto done) { client.listGames(std::move(done)); });
    REQUIRE(games);
    return *games;
}

/// Plays frames of 1/60 s until the condition holds.
bool play(EventLoop &loop, std::vector<RemoteSession *> sessions, const std::function<bool()> &condition)
{
    return waitUntil(loop, [&] {
        for (RemoteSession *session : sessions) {
            session->update(1.0f / 60.0f);
        }
        return condition();
    });
}

std::optional<game::EntityState> shipOf(const game::RemoteWorld &world, std::uint8_t slot)
{
    for (const auto &[id, entity] : world.entities()) {
        if (entity.kind == game::EntityKind::PlayerShip && entity.owner == slot) {
            return entity;
        }
    }
    return std::nullopt;
}

} // namespace

// --- The address of a server ---

TEST_CASE("A server is given as host:port, the port of the lobby by default", "[online]")
{
    using rtype::client::splitServer;
    using Parts = std::pair<std::string, std::uint16_t>;
    CHECK(splitServer("127.0.0.1:5000") == Parts{"127.0.0.1", 5000});
    CHECK(splitServer("rtype.example.com") == Parts{"rtype.example.com", 4242});
    CHECK(splitServer("[::1]:5000") == Parts{"::1", 5000});
    CHECK(splitServer("[::1]") == Parts{"::1", 4242});
    CHECK_FALSE(splitServer(""));
    CHECK_FALSE(splitServer(":5000"));
    CHECK_FALSE(splitServer("host:0"));
    CHECK_FALSE(splitServer("host:65536"));
    CHECK_FALSE(splitServer("host:abc"));
    CHECK_FALSE(splitServer("[::1"));
}

TEST_CASE("Every error of the master has a message for the player", "[online]")
{
    std::set<std::string> messages;
    for (std::uint16_t code = 1; code <= 12; ++code) {
        const std::string message = rtype::client::describe(static_cast<lobby::ErrorCode>(code));
        CHECK_FALSE(message.empty());
        messages.insert(message);
    }
    // HELLO_REQUIRED and ALREADY_WELCOMED share theirs: the player can do nothing about either
    CHECK(messages.size() == 11);
    CHECK_FALSE(rtype::client::describe(static_cast<lobby::ErrorCode>(999)).empty());
}

// --- The lobby ---

TEST_CASE("A client is welcomed, creates a game, and another one lists it and joins it", "[online]")
{
    Master master;
    Events aliceEvents;
    Events bobEvents;
    LobbyClient alice(master.loop, master.lobby.localAddress(), {master.identity.fingerprint()}, "Alice",
                      aliceEvents.handlers());
    LobbyClient bob(master.loop, master.lobby.localAddress(), {master.identity.fingerprint()}, "Bob",
                    bobEvents.handlers());

    REQUIRE(waitUntil(master.loop, [&] { return aliceEvents.welcomed && bobEvents.welcomed; }));
    CHECK(alice.welcomed());
    CHECK(alice.playerId() != bob.playerId());
    CHECK(list(master.loop, alice).empty());

    const lobby::GameJoinedMessage game = create(master.loop, alice, "Alice's game");
    const std::vector<lobby::GameInfo> games = list(master.loop, bob);
    REQUIRE(games.size() == 1);
    CHECK(games[0] == lobby::GameInfo{game.gameId, "Alice's game", 1, 2});

    const auto joined = answer<LobbyClient::JoinResult>(
        master.loop, [&](auto done) { bob.joinGame(game.gameId, std::move(done)); });
    REQUIRE(joined);
    CHECK(std::get<lobby::GameJoinedMessage>(*joined).udpPort == game.udpPort);

    // Errors reach the callback of their request
    const auto missing = answer<LobbyClient::JoinResult>(
        master.loop, [&](auto done) { alice.joinGame(999, std::move(done)); });
    REQUIRE(missing);
    CHECK(std::get<lobby::ErrorCode>(*missing) == lobby::ErrorCode::AlreadyInGame);

    bool left = false;
    bob.leaveGame([&] { left = true; });
    REQUIRE(waitUntil(master.loop, [&] { return left; }));
    CHECK(list(master.loop, alice)[0].players == 1);
    CHECK_FALSE(aliceEvents.closed);
}

TEST_CASE("A client refuses a server that does not have the pinned key", "[online]")
{
    Master master;
    Events events;
    const ServerIdentity other = ServerIdentity::generate();

    LobbyClient client(master.loop, master.lobby.localAddress(), {other.fingerprint()}, "Pavel",
                       events.handlers());

    REQUIRE(waitUntil(master.loop, [&] { return events.closed.has_value(); }));
    CHECK(events.closed->find("not the expected server") != std::string::npos);
    CHECK_FALSE(events.welcomed);
}

TEST_CASE("A client trusting no key learns the key of the server, then joins once it trusts it", "[online]")
{
    Master master;
    Events events;
    std::optional<rtype::network::KeyFingerprint> presented;
    LobbyClient::Handlers handlers = events.handlers();
    handlers.onUntrusted = [&](const rtype::network::KeyFingerprint &key) { presented = key; };

    LobbyClient first(master.loop, master.lobby.localAddress(), {}, "Pavel", std::move(handlers));

    REQUIRE(waitUntil(master.loop, [&] { return presented.has_value(); }));
    CHECK(*presented == master.identity.fingerprint());
    // Told instead of closed: the player is asked, not shown an error
    CHECK_FALSE(events.closed);
    CHECK_FALSE(first.welcomed());

    // The player trusts it: connected again, pinning it
    Events trusted;
    LobbyClient second(master.loop, master.lobby.localAddress(), {*presented}, "Pavel", trusted.handlers());
    CHECK(waitUntil(master.loop, [&] { return second.welcomed(); }));
}

TEST_CASE("A client tells why it could not join the lobby", "[online]")
{
    SECTION("Nobody at the address")
    {
        EventLoop loop;
        std::uint16_t port = 0;
        {
            const TcpListener closed(loop, Address::loopbackIpv4(0));
            port = closed.localAddress().port();
        }
        loop.poll();
        Events events;
        LobbyClient client(loop, Address::loopbackIpv4(port), {ServerIdentity::generate().fingerprint()},
                           "Pavel", events.handlers());
        REQUIRE(waitUntil(loop, [&] { return events.closed.has_value(); }));
        CHECK(events.closed->find("Cannot reach") != std::string::npos);
    }
    SECTION("A name that cannot be the name of a player, refused before connecting")
    {
        EventLoop loop;
        Events events;
        const std::vector pinned{ServerIdentity::generate().fingerprint()};
        CHECK_THROWS_AS(
            LobbyClient(loop, Address::loopbackIpv4(4242), pinned, std::string(17, 'p'), events.handlers()),
            std::invalid_argument);
        CHECK_THROWS_AS(LobbyClient(loop, Address::loopbackIpv4(4242), pinned, "", events.handlers()),
                        std::invalid_argument);
    }
}

// --- The game ---

TEST_CASE("Two clients play the same game online, each moving its own ship", "[online]")
{
    Master master;
    Events events;
    LobbyClient alice(master.loop, master.lobby.localAddress(), {master.identity.fingerprint()}, "Alice",
                      events.handlers());
    Events bobEvents;
    LobbyClient bob(master.loop, master.lobby.localAddress(), {master.identity.fingerprint()}, "Bob",
                    bobEvents.handlers());
    REQUIRE(waitUntil(master.loop, [&] { return alice.welcomed() && bob.welcomed(); }));
    const lobby::GameJoinedMessage aliceGame = create(master.loop, alice, "Duo");
    const auto bobJoined = answer<LobbyClient::JoinResult>(
        master.loop, [&](auto done) { bob.joinGame(aliceGame.gameId, std::move(done)); });
    REQUIRE(bobJoined);

    RemoteSession aliceSession(master.loop, Address::loopbackIpv4(aliceGame.udpPort), aliceGame);
    RemoteSession bobSession(master.loop, Address::loopbackIpv4(aliceGame.udpPort),
                             std::get<lobby::GameJoinedMessage>(*bobJoined));
    const std::vector<RemoteSession *> both = {&aliceSession, &bobSession};

    // Both in, both see both ships, and know each other's name
    REQUIRE(play(master.loop, both, [&] {
        return shipOf(aliceSession.world(), 1).has_value() && shipOf(bobSession.world(), 0).has_value() &&
               aliceSession.names().size() == 2;
    }));
    CHECK(aliceSession.localSlot() == 0);
    CHECK(bobSession.localSlot() == 1);
    CHECK(aliceSession.names().at(1) == "Bob");

    // takeSnapshots gives, at every frame, the world drawn 100 ms in the past: both ships once it gets there
    REQUIRE(
        play(master.loop, both, [&] { return aliceSession.interpolation().view().entities.size() >= 2; }));
    for (int frame = 0; frame < 3; ++frame) {
        const std::vector<game::Snapshot> drawn = aliceSession.takeSnapshots();
        REQUIRE(drawn.size() == 1);
        CHECK(drawn[0].entities.size() >= 2);
        CHECK(drawn[0].tick < aliceSession.world().tick());
    }

    // Bob holds Right: his ship moves, on both screens, Alice's does not
    const float bobStart = shipOf(aliceSession.world(), 1)->x;
    const float aliceStart = shipOf(aliceSession.world(), 0)->x;
    game::PlayerInput right;
    right.set(game::PlayerInput::Right, true);
    bobSession.setInput(right);
    // Predicted: Bob's ship moves on his screen at the next input sent, before the game has it
    REQUIRE(bobSession.prediction().predicted());
    const float predictedStart = bobSession.prediction().predicted()->x;
    const float gameStart = shipOf(bobSession.world(), 1)->x;
    REQUIRE(play(master.loop, {&bobSession},
                 [&] { return bobSession.prediction().predicted()->x > predictedStart; }));
    CHECK(shipOf(bobSession.world(), 1)->x == gameStart);
    REQUIRE(play(master.loop, both, [&] {
        return shipOf(aliceSession.world(), 1)->x > bobStart + 100.0f &&
               shipOf(bobSession.world(), 1)->x > bobStart + 100.0f;
    }));
    CHECK(shipOf(aliceSession.world(), 0)->x == aliceStart);
    // Its inputs are stamped ahead of the world
    CHECK(bobSession.inputTick() > bobSession.world().tick());

    // Bob leaves: his session ends, Alice's game goes on
    bool left = false;
    bob.leaveGame([&] { left = true; });
    REQUIRE(play(master.loop, both, [&] { return left && bobSession.isOver(); }));
    CHECK(bobSession.endReason() == "Left the game");
    CHECK_FALSE(aliceSession.isOver());
}

TEST_CASE("A client is told when its game crashes", "[online]")
{
    Master master;
    Events events;
    LobbyClient alice(master.loop, master.lobby.localAddress(), {master.identity.fingerprint()}, "Alice",
                      events.handlers());
    REQUIRE(waitUntil(master.loop, [&] { return alice.welcomed(); }));
    const lobby::GameJoinedMessage game = create(master.loop, alice, "Fragile");

    testing::killProcess(*master.games.processId(game.gameId));

    REQUIRE(waitUntil(master.loop, [&] { return !events.left.empty(); }));
    CHECK(events.left[0] == lobby::LeftGameMessage{game.gameId, lobby::LeftGameReason::GameCrashed});
    // Back in the lobby: the client can create another one
    (void)create(master.loop, alice, "Sturdy");
}

TEST_CASE("A game that stops ends the session of its players", "[online]")
{
    Master master;
    Events events;
    LobbyClient alice(master.loop, master.lobby.localAddress(), {master.identity.fingerprint()}, "Alice",
                      events.handlers());
    REQUIRE(waitUntil(master.loop, [&] { return alice.welcomed(); }));
    const lobby::GameJoinedMessage game = create(master.loop, alice, "Short");
    RemoteSession session(master.loop, Address::loopbackIpv4(game.udpPort), game);
    REQUIRE(play(master.loop, {&session}, [&] { return session.accepted(); }));

    // The master stops: its games tell their players
    master.games.shutdown();

    REQUIRE(play(master.loop, {&session}, [&] { return session.isOver(); }));
    CHECK(session.endReason() == "The game stopped");
}
