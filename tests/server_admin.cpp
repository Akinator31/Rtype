// Tests of the administration of the master (docs/architecture/administration.md,
// docs/protocol/admin-api.md): Administration alone, with fakes of the games and the lobby; what it keeps in
// the database, on every engine; then end to end, over HTTP and the WebSocket, with real lobby clients and
// real instances.

#include "Common/Secret.hpp"
#include "GameServers.hpp"
#include "Http.hpp"
#include "LobbyClient.hpp"
#include "Master/AddressBans.hpp"
#include "Master/AdminStore.hpp"
#include "Master/Administration.hpp"
#include "Master/Games.hpp"
#include "Master/Lobby.hpp"
#include "Master/ServerDatabase.hpp"
#include "TestDatabase.hpp"
#include "Web/LocalAccess.hpp"
#include "Web/WebServer.hpp"
#include "WebSocketClient.hpp"
#include "rtype/database/DatabaseThread.hpp"
#include "rtype/game/Messages.hpp"
#include "rtype/lobby/Messages.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/Datagram.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/Session.hpp"
#include "rtype/network/TcpListener.hpp"
#include "rtype/network/Tls.hpp"
#include "rtype/network/UdpSocket.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <crow/json.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <format>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

using namespace rtype::server;
using namespace std::chrono_literals;
using Catch::Matchers::ContainsSubstring;
using rtype::lobby::ErrorCode;
using rtype::lobby::ErrorMessage;
using rtype::network::Address;
using rtype::network::EventLoop;
using testing::HttpRequest;
using testing::HttpResponse;
using testing::waitUntil;
using Clock = std::chrono::system_clock;

namespace lobby = rtype::lobby;

namespace
{

// --- Fakes of the games and the lobby ---

struct FakeGames final : AdminGames {
    std::vector<AdminGame> games;
    std::vector<std::uint32_t> kicked;
    std::vector<std::uint32_t> ended;

    [[nodiscard]] std::vector<AdminGame> adminGames() const override { return games; }
    bool kick(std::uint32_t playerId) override
    {
        for (AdminGame &game : games) {
            if (std::erase_if(game.players,
                              [&](const AdminGamePlayer &player) { return player.id == playerId; }) != 0) {
                kicked.push_back(playerId);
                return true;
            }
        }
        return false;
    }
    bool end(std::uint32_t gameId) override
    {
        if (std::erase_if(games, [&](const AdminGame &game) { return game.id == gameId; }) == 0) {
            return false;
        }
        ended.push_back(gameId);
        return true;
    }
};

struct FakeLobby final : AdminLobby {
    std::vector<AdminPlayer> players;
    std::vector<std::pair<std::uint32_t, ErrorCode>> disconnected;
    std::vector<std::pair<std::uint32_t, std::string>> messages;

    [[nodiscard]] std::vector<AdminPlayer> adminPlayers() const override { return players; }
    bool disconnect(std::uint32_t playerId, ErrorCode code) override
    {
        if (std::erase_if(players, [&](const AdminPlayer &player) { return player.id == playerId; }) == 0) {
            return false;
        }
        disconnected.emplace_back(playerId, code);
        return true;
    }
    bool sendMessage(std::uint32_t playerId, const std::string &text) override
    {
        const bool known =
            std::ranges::any_of(players, [&](const AdminPlayer &player) { return player.id == playerId; });
        if (known) {
            messages.emplace_back(playerId, text);
        }
        return known;
    }
};

Address ip(const std::string &text)
{
    return *Address::parse(text, 0);
}

AdminPlayer adminPlayer(std::uint32_t id, const std::string &name, const std::string &address,
                        std::optional<std::uint32_t> gameId = std::nullopt)
{
    return {id, name, ip(address), gameId, Clock::now()};
}

AdminGame adminGame(std::uint32_t id, const std::string &name, std::vector<AdminGamePlayer> players)
{
    AdminGame game;
    game.id = id;
    game.name = name;
    game.maxPlayers = 4;
    game.port = static_cast<std::uint16_t>(4242 + id);
    game.players = std::move(players);
    game.createdAt = Clock::now();
    return game;
}

/// Administration with fakes, its database on a thread whose results come back on the loop.
struct Fixture {
    EventLoop loop;
    rtype::test::TestDatabase test;
    FakeGames games;
    FakeLobby lobby;
    AddressBans bans;
    std::optional<rtype::database::DatabaseThread> database;
    std::optional<Administration> administration;
    std::vector<AdminEvent> events;

    Fixture(rtype::database::Engine engine, std::string_view name) : test(engine, name)
    {
        rtype::database::Database opened = test.open();
        opened.migrate(ServerMigrations);
        database.emplace(std::move(opened),
                         [this](std::function<void()> task) { loop.post(std::move(task)); });
        administration.emplace(loop, games, lobby, bans, &*database,
                               AdministrationSettings{.version = "1.2.3",
                                                      .lobbyPort = 4242,
                                                      .gamePort = 4242,
                                                      .maxGames = 57,
                                                      .panelPort = 8080,
                                                      .admins = {}});
        administration->subscribe([this](const AdminEvent &event) { events.push_back(event); });
    }
    ~Fixture()
    {
        administration.reset();
        database.reset();
    }
    Fixture(const Fixture &) = delete;
    Fixture &operator=(const Fixture &) = delete;

    /// The actions recorded so far, once `count` came back as events.
    std::vector<AdminAction> recorded(std::size_t count)
    {
        std::vector<AdminAction> actions;
        REQUIRE(waitUntil(loop, [&] {
            actions.clear();
            for (const AdminEvent &event : events) {
                if (const auto *action = std::get_if<admin_event::ActionRecorded>(&event)) {
                    actions.push_back(action->action);
                }
            }
            return actions.size() >= count;
        }));
        return actions;
    }

    /// The record, read from the database as GET /api/actions does.
    std::vector<AdminAction> read(std::size_t limit = 100, std::optional<long long> before = std::nullopt)
    {
        std::optional<std::optional<std::vector<AdminAction>>> result;
        administration->actions(limit, before, [&](std::optional<std::vector<AdminAction>> actions) {
            result = std::move(actions);
        });
        REQUIRE(waitUntil(loop, [&] { return result.has_value(); }));
        REQUIRE(result->has_value());
        return **result;
    }
};

crow::json::rvalue parse(const std::string &text)
{
    crow::json::rvalue json = crow::json::load(text);
    INFO(text);
    REQUIRE(json);
    return json;
}

} // namespace

// --- Administration alone ---

TEST_CASE("A subscriber gets a snapshot of the server, then its events in order", "[server][admin]")
{
    Fixture fixture(rtype::database::Engine::Sqlite, "admin_events");
    fixture.lobby.players = {adminPlayer(1, "Alice", "203.0.113.9", 7)};
    fixture.games.games = {adminGame(7, "Bydo hunters", {{1, "Alice"}})};

    std::vector<AdminEvent> events;
    const std::uint64_t subscription =
        fixture.administration->subscribe([&](const AdminEvent &event) { events.push_back(event); });
    REQUIRE(events.size() == 1);
    const auto &snapshot = std::get<admin_event::Snapshot>(events[0]);
    CHECK(snapshot.server.version == "1.2.3");
    CHECK(snapshot.server.lobbyPort == 4242);
    CHECK(snapshot.server.gamePort == 4242);
    CHECK(snapshot.server.maxGames == 57);
    CHECK(snapshot.server.panelPort == 8080);
    CHECK(snapshot.server.players == 1);
    CHECK(snapshot.server.games == 1);
    CHECK(snapshot.games == fixture.games.games);
    CHECK(snapshot.players == fixture.lobby.players);

    // Each call of the lobby and the games, read back from them
    fixture.lobby.players.push_back(adminPlayer(2, "Bob", "198.51.100.4"));
    fixture.administration->playerJoined(2);
    fixture.lobby.players[1].gameId = 7;
    fixture.games.games[0].players.push_back({2, "Bob"});
    fixture.administration->playerChanged(2);
    fixture.administration->gameChanged(7);
    fixture.games.games.push_back(adminGame(8, "Solo", {}));
    fixture.administration->gameStarted(8);
    fixture.administration->gameEnded(8, GameEnd::Crashed);
    fixture.administration->playerLeft(2);

    REQUIRE(events.size() == 7);
    CHECK(std::get<admin_event::PlayerJoined>(events[1]).player.name == "Bob");
    CHECK(std::get<admin_event::PlayerUpdated>(events[2]).player.gameId == 7U);
    CHECK(std::get<admin_event::GameUpdated>(events[3]).game.players.size() == 2);
    CHECK(std::get<admin_event::GameStarted>(events[4]).game.name == "Solo");
    CHECK(std::get<admin_event::GameEnded>(events[5]).gameId == 8);
    CHECK(std::get<admin_event::GameEnded>(events[5]).reason == GameEnd::Crashed);
    CHECK(std::get<admin_event::PlayerLeft>(events[6]).playerId == 2);

    // Unknown ids say nothing; an unsubscribed subscriber hears nothing more
    fixture.administration->playerJoined(99);
    fixture.administration->gameChanged(99);
    CHECK(events.size() == 7);
    fixture.administration->unsubscribe(subscription);
    fixture.administration->playerLeft(1);
    CHECK(events.size() == 7);
    CHECK(fixture.administration->subscriberCount() == 1);
}

TEST_CASE("Each action goes through the games and the lobby, and is recorded", "[server][admin][database]")
{
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    Fixture fixture(engine, "admin_actions");
    fixture.lobby.players = {adminPlayer(1, "Alice", "203.0.113.9", 7),
                             adminPlayer(2, "Bob", "198.51.100.4", 7),
                             adminPlayer(3, "Carol", "198.51.100.4"), adminPlayer(4, "Dave", "192.0.2.1")};
    fixture.games.games = {adminGame(7, "Bydo hunters", {{1, "Alice"}, {2, "Bob"}}),
                           adminGame(8, "Solo", {})};
    Administration &administration = *fixture.administration;

    // Removed: from its game, then from the lobby with KICKED
    CHECK(administration.kick(1, "cheating", "local"));
    CHECK(fixture.games.kicked == std::vector<std::uint32_t>{1});
    CHECK(fixture.lobby.disconnected ==
          std::vector<std::pair<std::uint32_t, ErrorCode>>{{1, ErrorCode::Kicked}});
    // A message to a game, then to everyone
    CHECK(administration.sendMessage(7, "Last wave", "local") == 1U);
    CHECK(administration.sendMessage(std::nullopt, "Restart in 5 minutes", "local") == 3U);
    CHECK(fixture.lobby.messages.size() == 4);
    CHECK(fixture.lobby.messages[0] == std::pair<std::uint32_t, std::string>{2, "Last wave"});
    // A game ended
    CHECK(administration.endGame(7, "", "local"));
    CHECK(fixture.games.ended == std::vector<std::uint32_t>{7});
    // An address banned: the players connected from it are removed with BANNED, the lobby refuses it
    CHECK(administration.ban(ip("198.51.100.4"), 1h, "flood", "local") == 2);
    CHECK(fixture.lobby.disconnected.size() == 3);
    CHECK(fixture.lobby.disconnected[1] == std::pair<std::uint32_t, ErrorCode>{2, ErrorCode::Banned});
    CHECK(fixture.lobby.disconnected[2] == std::pair<std::uint32_t, ErrorCode>{3, ErrorCode::Banned});
    CHECK(fixture.bans.find(ip("198.51.100.4")));
    CHECK(fixture.bans.find(ip("::ffff:198.51.100.4")));
    CHECK_FALSE(fixture.bans.find(ip("198.51.100.5")));

    // What is not there is refused, and not recorded
    CHECK_FALSE(administration.kick(99, "", "local"));
    CHECK_FALSE(administration.endGame(99, "", "local"));
    CHECK_FALSE(administration.endGame(7, "", "local"));
    CHECK_FALSE(administration.sendMessage(99, "Hello", "local"));

    // Each action comes back as an event once recorded, with its id
    const std::vector<AdminAction> recorded = fixture.recorded(5);
    REQUIRE(recorded.size() == 5);
    for (std::size_t i = 0; i < recorded.size(); ++i) {
        CHECK(recorded[i].id == static_cast<long long>(i + 1));
        CHECK(recorded[i].actor == "local");
    }
    CHECK(recorded[0].action == "kick");
    CHECK(recorded[0].target == "player:1");
    const crow::json::rvalue kick = parse(recorded[0].details);
    CHECK(kick["name"].s() == "Alice");
    CHECK(kick["address"].s() == "203.0.113.9");
    CHECK(kick["gameId"].i() == 7);
    CHECK(kick["reason"].s() == "cheating");
    CHECK(recorded[1].target == "game:7");
    CHECK(parse(recorded[1].details)["recipients"].i() == 1);
    CHECK(recorded[2].target == "everyone");
    CHECK(parse(recorded[2].details)["text"].s() == "Restart in 5 minutes");
    CHECK(recorded[3].action == "end_game");
    CHECK(parse(recorded[3].details)["name"].s() == "Bydo hunters");
    CHECK(recorded[4].action == "ban");
    CHECK(recorded[4].target == "address:198.51.100.4");
    const crow::json::rvalue ban = parse(recorded[4].details);
    CHECK(ban["duration"].i() == 3600);
    CHECK(ban["removed"].i() == 2);
    CHECK(ban["reason"].s() == "flood");

    // The record, newest first, page by page
    const std::vector<AdminAction> all = fixture.read();
    REQUIRE(all.size() == 5);
    CHECK(all.front() == recorded.back());
    CHECK(all.back() == recorded.front());
    const std::vector<AdminAction> page = fixture.read(2, 4);
    REQUIRE(page.size() == 2);
    CHECK(page[0].id == 3);
    CHECK(page[1].id == 2);
    CHECK(fixture.read(10, 1).empty());

    // The ban is in the database, for the next start
    rtype::database::Database database = fixture.test.open();
    const std::vector<AddressBan> kept = loadAddressBans(database, Clock::now());
    REQUIRE(kept.size() == 1);
    CHECK(kept[0].address == ip("198.51.100.4"));
    CHECK(kept[0].reason == "flood");
    CHECK(kept[0].actor == "local");
}

TEST_CASE("A ban lifted before it expires lets the address in again, and is recorded",
          "[server][admin][database]")
{
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    Fixture fixture(engine, "admin_unban");
    Administration &administration = *fixture.administration;
    (void)administration.ban(ip("198.51.100.4"), 1h, "flood", "local");
    (void)administration.ban(ip("192.0.2.1"), 1h, "", "local");

    // Lifted, written as the lobby saw it
    CHECK(administration.unban(ip("::ffff:198.51.100.4"), "a mistake", "local"));
    CHECK_FALSE(fixture.bans.find(ip("198.51.100.4")));
    CHECK(fixture.bans.find(ip("192.0.2.1")));
    // Not banned any more, or never: refused, and not recorded
    CHECK_FALSE(administration.unban(ip("198.51.100.4"), "", "local"));
    CHECK_FALSE(administration.unban(ip("203.0.113.9"), "", "local"));

    const std::vector<AdminAction> recorded = fixture.recorded(3);
    REQUIRE(recorded.size() == 3);
    CHECK(recorded[2].action == "unban");
    CHECK(recorded[2].target == "address:198.51.100.4");
    CHECK(recorded[2].actor == "local");
    const crow::json::rvalue details = parse(recorded[2].details);
    CHECK(details["reason"].s() == "a mistake");
    CHECK(details["bannedBy"].s() == "local");
    CHECK(details["expiresAt"].t() == crow::json::type::Number);
    CHECK(fixture.read().size() == 3);

    // Gone from the database too: the next start does not ban it again
    rtype::database::Database database = fixture.test.open();
    const std::vector<AddressBan> kept = loadAddressBans(database, Clock::now());
    REQUIRE(kept.size() == 1);
    CHECK(kept[0].address == ip("192.0.2.1"));
}

TEST_CASE("A ban lasts seven days at most", "[server][admin]")
{
    Fixture fixture(rtype::database::Engine::Sqlite, "admin_ban_limit");
    const Clock::time_point before = Clock::now();
    (void)fixture.administration->ban(ip("192.0.2.1"), std::chrono::hours(24 * 30), "", "local");
    const std::optional<AddressBan> ban = fixture.bans.find(ip("192.0.2.1"));
    REQUIRE(ban);
    CHECK(ban->expiresAt - before >= MaxBanDuration);
    CHECK(ban->expiresAt - before < MaxBanDuration + 1min);
}

// --- The bans ---

TEST_CASE("A ban runs until it expires, whatever the port and the writing of its address", "[server][admin]")
{
    const Clock::time_point now = Clock::now();
    AddressBans bans({{ip("203.0.113.9"), now, now + 1h, "flood", "local"}});
    CHECK(bans.find(*Address::parse("203.0.113.9", 51000), now));
    CHECK(bans.find(ip("::ffff:203.0.113.9"), now));
    CHECK_FALSE(bans.find(ip("203.0.113.10"), now));
    CHECK_FALSE(bans.find(ip("203.0.113.9"), now + 1h));
    CHECK(banKey(ip("::ffff:203.0.113.9")) == ip("203.0.113.9"));
    CHECK(banKey(ip("2001:db8::1")) == ip("2001:db8::1"));

    // Added again: replaced
    bans.add({*Address::parse("::ffff:203.0.113.9", 4000), now, now + 2h, "again", "local"});
    REQUIRE(bans.list(now).size() == 1);
    CHECK(bans.list(now)[0].reason == "again");
    CHECK(bans.list(now)[0].address == ip("203.0.113.9"));

    bans.add({ip("2001:db8::1"), now, now + 30min, "", "local"});
    const std::vector<AddressBan> listed = bans.list(now);
    REQUIRE(listed.size() == 2);
    CHECK(listed[0].address == ip("2001:db8::1"));
    CHECK(bans.purge(now + 1h) == 1);
    CHECK(bans.list(now).size() == 1);

    // Lifted before it expires, whatever the writing of its address; once only
    const std::optional<AddressBan> lifted = bans.remove(*Address::parse("::ffff:203.0.113.9", 4000), now);
    REQUIRE(lifted);
    CHECK(lifted->reason == "again");
    CHECK_FALSE(bans.find(ip("203.0.113.9"), now));
    CHECK_FALSE(bans.remove(ip("203.0.113.9"), now));
    // An expired ban is not lifted: it had ended already
    bans.add({ip("192.0.2.1"), now - 2h, now - 1h, "", "local"});
    CHECK_FALSE(bans.remove(ip("192.0.2.1"), now));
    CHECK(bans.list(now).empty());
}

TEST_CASE("The bans are kept in the database, and the expired ones purged", "[server][admin][database]")
{
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    const rtype::test::TestDatabase test(engine, "admin_bans");
    rtype::database::Database database = test.open();
    database.migrate(ServerMigrations);
    const Clock::time_point now = fromUnixMilliseconds(unixMilliseconds(Clock::now()));

    saveAddressBan(database, {ip("203.0.113.9"), now, now + 1h, "flood", "local"});
    saveAddressBan(database, {ip("2001:db8::1"), now, now + 1min, "", "local"});
    saveAddressBan(database, {ip("::ffff:192.0.2.1"), now - 2h, now - 1h, "old", "local"});
    // The same address again: replaced
    saveAddressBan(database, {ip("203.0.113.9"), now, now + 2h, "again", "local"});

    const std::vector<AddressBan> loaded = loadAddressBans(database, now);
    REQUIRE(loaded.size() == 2);
    CHECK(loaded[0] == AddressBan{ip("2001:db8::1"), now, now + 1min, "", "local"});
    CHECK(loaded[1] == AddressBan{ip("203.0.113.9"), now, now + 2h, "again", "local"});
    // The expired one is gone from the table
    long long rows = 0;
    database.session() << "SELECT COUNT(*) FROM address_bans", soci::into(rows);
    CHECK(rows == 2);

    CHECK(purgeAddressBans(database, now + 1h) == 1);
    CHECK(loadAddressBans(database, now + 1h).size() == 1);
}

// --- End to end ---

namespace
{

/// A client of a game, over UDP, with the keys of its GAME_JOINED.
struct GameClient {
    rtype::network::UdpSocket socket;
    Address game;
    rtype::network::ClientSession session;
    bool accepted = false;
    std::optional<rtype::network::SessionEnd> end;

    GameClient(EventLoop &loop, const lobby::GameJoinedMessage &joined)
        : socket(loop, Address::loopbackIpv4(0)), game(Address::loopbackIpv4(joined.udpPort)),
          session(joined.sessionId, rtype::network::DatagramKey(joined.clientKey),
                  rtype::network::DatagramKey(joined.serverKey),
                  [this](std::span<const std::byte> datagram) { socket.send(game, datagram); })
    {
        socket.startReceiving([this](const Address &from, std::span<const std::byte> datagram) {
            if (from == game) {
                session.receive(datagram, std::chrono::steady_clock::now());
            }
        });
        session.start([this](std::span<const std::byte>) { accepted = true; },
                      [](std::uint8_t, std::span<const std::byte>) {},
                      [this](rtype::network::SessionEnd ended) { end = ended; },
                      std::chrono::steady_clock::now());
    }

    void update()
    {
        if (!end) {
            session.update(std::chrono::steady_clock::now());
        }
    }
};

/// A master as runMaster() puts it together, on the loop of the test: its games run by real instances, its
/// lobby, its administration on a database, and its web server.
struct AdminMaster {
    EventLoop loop;
    rtype::network::ServerIdentity identity;
    const rtype::test::TestDatabase &test;
    std::optional<rtype::database::DatabaseThread> database;
    AddressBans bans;
    Relay relay{Address::loopbackIpv4(0)};
    Games games;
    Lobby lobby;
    std::optional<Administration> administration;
    Secret token = newAccessToken();
    std::optional<WebServer> web;
    std::string base;
    std::vector<GameClient *> gameClients;

    AdminMaster(const rtype::test::TestDatabase &database_, rtype::network::ServerIdentity key)
        : identity(std::move(key)), test(database_), bans(loadBans(database_)),
          games(loop, relay, testing::instances()),
          lobby(loop, rtype::network::TcpListener(loop, Address::loopbackIpv4(0)), identity, games)
    {
        rtype::database::Database opened = test.open();
        opened.migrate(ServerMigrations);
        database.emplace(std::move(opened),
                         [this](std::function<void()> task) { loop.post(std::move(task)); });
        WebServerSettings settings;
        settings.listen = Address::loopbackIpv4(0);
        settings.accessToken = token;
        web.emplace(loop, settings);
        base = std::format("http://{}", web->localAddress());
        administration.emplace(loop, games, lobby, bans, &*database,
                               AdministrationSettings{.version = "test",
                                                      .lobbyPort = lobby.localAddress().port(),
                                                      .gamePort = relay.publicAddress().port(),
                                                      .maxGames = 4,
                                                      .panelPort = web->localAddress().port(),
                                                      .admins = {}});
        lobby.setBans(&bans);
        games.setObserver(&*administration);
        lobby.setObserver(&*administration);
        web->setAdministration(&*administration);
    }

    ~AdminMaster()
    {
        // Declared after the master, the game clients are gone already
        gameClients.clear();
        web->stop();
        lobby.close();
        games.shutdown();
        (void)wait([&] { return games.empty(); });
        web->setAdministration(nullptr);
        lobby.setObserver(nullptr);
        games.setObserver(nullptr);
        administration.reset();
        database.reset();
    }

    AdminMaster(const AdminMaster &) = delete;
    AdminMaster &operator=(const AdminMaster &) = delete;

    static std::vector<AddressBan> loadBans(const rtype::test::TestDatabase &test)
    {
        rtype::database::Database opened = test.open();
        opened.migrate(ServerMigrations);
        return loadAddressBans(opened, Clock::now());
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

    /// A request to the web server, the loop running meanwhile: the API answers from it.
    HttpResponse call(const std::string &path, const HttpRequest &request = {}, const std::string &body = {})
    {
        auto answer =
            std::async(std::launch::async, [&] { return testing::http(base + path, request, body); });
        REQUIRE(wait([&] { return answer.wait_for(0ms) == std::future_status::ready; }));
        return answer.get();
    }

    /// Opens the panel with the token: the session cookie, and its CSRF token.
    std::pair<std::string, std::string> signIn()
    {
        const HttpResponse opened = call("/?token=" + formatSecret(token));
        REQUIRE(opened.status == 303);
        const std::string cookie = opened.header("set-cookie");
        const std::string session = cookie.substr(0, cookie.find(';'));
        const HttpResponse described =
            call("/api/session", {.method = "GET", .headers = {"Cookie: " + session}});
        REQUIRE(described.status == 200);
        return {session, parse(described.body)["csrfToken"].s()};
    }

    std::unique_ptr<testing::LobbyClient> player(const std::string &name)
    {
        auto created =
            std::make_unique<testing::LobbyClient>(loop, lobby.localAddress(), identity.fingerprint());
        REQUIRE(waitUntil(loop, [&] { return created->connection.has_value(); }));
        created->hello(name);
        REQUIRE(waitUntil(loop, [&] { return !created->received.empty(); }));
        REQUIRE(created->received[0].first == lobby::message_type::Welcome);
        return created;
    }

    /// Reply number `index` of a client, which must be GAME_JOINED.
    lobby::GameJoinedMessage joined(testing::LobbyClient &client, std::size_t index)
    {
        REQUIRE(wait([&] { return client.received.size() > index; }));
        REQUIRE(client.received[index].first == lobby::message_type::GameJoined);
        return *lobby::decodeGameJoined(client.received[index].second);
    }
};

/// A session of the panel: GET with its cookie, POST with its cookie and its CSRF token.
struct Panel {
    AdminMaster &master;
    std::string session;
    std::string csrf;

    explicit Panel(AdminMaster &opened) : master(opened) { std::tie(session, csrf) = master.signIn(); }

    HttpResponse get(const std::string &path) const
    {
        return master.call(path, {.method = "GET", .headers = {"Cookie: " + session}});
    }

    HttpResponse post(const std::string &path, const std::string &body = "") const
    {
        return master.call(
            path,
            {.method = "POST",
             .headers = {"Cookie: " + session, "X-CSRF-Token: " + csrf, "Content-Type: application/json"}},
            body);
    }

    std::unique_ptr<testing::WebSocketClient> events() const
    {
        return std::make_unique<testing::WebSocketClient>(
            master.loop, master.web->localAddress(),
            std::vector<std::string>{"Cookie: " + session,
                                     std::format("Origin: http://{}", master.web->localAddress())});
    }
};

/// The events of a WebSocket of the given type, parsed.
std::vector<crow::json::rvalue> eventsOf(const testing::WebSocketClient &socket, const std::string &type)
{
    std::vector<crow::json::rvalue> found;
    for (const std::string &message : socket.messages) {
        crow::json::rvalue event = crow::json::load(message);
        if (event && event["type"].s() == type) {
            found.push_back(std::move(event));
        }
    }
    return found;
}

} // namespace

TEST_CASE("From the panel, a host sees the server, its games and its players, live", "[server][admin]")
{
    const rtype::test::TestDatabase test(rtype::database::Engine::Sqlite, "admin_live");
    AdminMaster master(test, rtype::network::ServerIdentity::generate());
    const Panel panel(master);

    // The WebSocket: a snapshot first
    const auto socket = panel.events();
    REQUIRE(master.wait([&] { return !socket->messages.empty(); }));
    CHECK(socket->status == 101);
    const crow::json::rvalue snapshot = parse(socket->messages[0]);
    CHECK(snapshot["type"].s() == "snapshot");
    CHECK(snapshot["server"]["version"].s() == "test");
    CHECK(snapshot["games"].size() == 0);
    CHECK(snapshot["players"].size() == 0);

    // Alice comes, and creates a game: the events follow
    auto alice = master.player("Alice");
    alice->send(lobby::message_type::CreateGame, lobby::encode(lobby::CreateGameMessage{"Bydo hunters", 2}));
    const lobby::GameJoinedMessage game = master.joined(*alice, 1);
    REQUIRE(master.wait([&] {
        return !eventsOf(*socket, "player.joined").empty() && !eventsOf(*socket, "game.started").empty() &&
               !eventsOf(*socket, "player.updated").empty();
    }));
    CHECK(eventsOf(*socket, "player.joined")[0]["player"]["name"].s() == "Alice");
    CHECK(eventsOf(*socket, "game.started")[0]["game"]["name"].s() == "Bydo hunters");
    CHECK(eventsOf(*socket, "player.updated")[0]["player"]["gameId"].i() == game.gameId);
    // Its instance reports every second
    GameClient client(master.loop, game);
    master.gameClients.push_back(&client);
    REQUIRE(master.wait([&] { return client.accepted; }));
    REQUIRE(master.wait([&] {
        for (const crow::json::rvalue &updated : eventsOf(*socket, "game.updated")) {
            if (updated["game"]["tick"].t() == crow::json::type::Number) {
                return true;
            }
        }
        return false;
    }));

    // The routes
    const crow::json::rvalue server = parse(panel.get("/api/server").body);
    CHECK(server["version"].s() == "test");
    CHECK(server["ports"]["lobby"].i() == master.lobby.localAddress().port());
    CHECK(server["ports"]["game"].i() == game.udpPort);
    CHECK(server["ports"]["panel"].i() == master.web->localAddress().port());
    CHECK(server["maxGames"].i() == 4);
    CHECK(server["players"].i() == 1);
    CHECK(server["games"].i() == 1);
    const crow::json::rvalue games = parse(panel.get("/api/games").body);
    REQUIRE(games.size() == 1);
    CHECK(games[0]["id"].i() == game.gameId);
    CHECK(games[0]["maxPlayers"].i() == 2);
    CHECK(games[0]["port"].i() == game.udpPort);
    REQUIRE(games[0]["players"].size() == 1);
    CHECK(games[0]["players"][0]["name"].s() == "Alice");
    const crow::json::rvalue one = parse(panel.get(std::format("/api/games/{}", game.gameId)).body);
    CHECK(one["name"].s() == "Bydo hunters");
    CHECK(panel.get("/api/games/999").status == 404);
    CHECK(panel.get("/api/games/99999999999").status == 404);
    const crow::json::rvalue players = parse(panel.get("/api/players").body);
    REQUIRE(players.size() == 1);
    CHECK(players[0]["name"].s() == "Alice");
    CHECK(players[0]["address"].s() == "127.0.0.1");
    CHECK(players[0]["gameId"].i() == game.gameId);
    CHECK(parse(panel.get("/api/actions").body).size() == 0);
    CHECK(parse(panel.get("/api/bans").body).size() == 0);

    // Alice leaves
    alice.reset();
    REQUIRE(master.wait([&] {
        return !eventsOf(*socket, "player.left").empty() && !eventsOf(*socket, "game.ended").empty();
    }));
    CHECK(eventsOf(*socket, "game.ended")[0]["reason"].s() == "empty");
}

TEST_CASE("A player removed from the panel is told, and so is its instance", "[server][admin]")
{
    const rtype::test::TestDatabase test(rtype::database::Engine::Sqlite, "admin_kick");
    AdminMaster master(test, rtype::network::ServerIdentity::generate());
    const Panel panel(master);
    const auto socket = panel.events();
    auto alice = master.player("Alice");
    auto bob = master.player("Bob");
    alice->send(lobby::message_type::CreateGame, lobby::encode(lobby::CreateGameMessage{"Duo", 2}));
    const lobby::GameJoinedMessage game = master.joined(*alice, 1);
    bob->send(lobby::message_type::JoinGame, lobby::encode(lobby::JoinGameMessage{game.gameId}));
    GameClient aliceClient(master.loop, game);
    GameClient bobClient(master.loop, master.joined(*bob, 1));
    master.gameClients = {&aliceClient, &bobClient};
    REQUIRE(master.wait([&] { return aliceClient.accepted && bobClient.accepted; }));
    const std::uint32_t bobId = lobby::decodeWelcome(bob->received[0].second)->playerId;

    const HttpResponse kicked =
        panel.post(std::format("/api/players/{}/kick", bobId), R"({"reason":"cheating"})");
    CHECK(kicked.status == 200);
    CHECK(parse(kicked.body)["playerId"].i() == bobId);

    // Bob: out of the game, then out of the server, with the reasons
    REQUIRE(master.wait([&] { return bob->closed.has_value() && bobClient.end.has_value(); }));
    REQUIRE(bob->received.size() >= 4);
    CHECK(bob->received[2].first == lobby::message_type::LeftGame);
    CHECK(lobby::decodeLeftGame(bob->received[2].second) ==
          lobby::LeftGameMessage{game.gameId, lobby::LeftGameReason::Kicked});
    CHECK(bob->received[3].first == lobby::message_type::Error);
    CHECK(lobby::decodeError(bob->received[3].second) == ErrorMessage{0, ErrorCode::Kicked});
    // The instance removed Bob's session; Alice plays on
    CHECK(bobClient.end->reason == rtype::network::DisconnectReason::Quit);
    CHECK_FALSE(aliceClient.end);
    CHECK(master.games.adminGames().at(0).players.size() == 1);

    // Recorded, and told to the panel
    REQUIRE(master.wait([&] { return !eventsOf(*socket, "action").empty(); }));
    const crow::json::rvalue actions = parse(panel.get("/api/actions").body);
    REQUIRE(actions.size() == 1);
    CHECK(actions[0]["action"].s() == "kick");
    CHECK(actions[0]["actor"].s() == "local");
    CHECK(actions[0]["target"].s() == std::format("player:{}", bobId));
    CHECK(actions[0]["details"]["name"].s() == "Bob");
    CHECK(actions[0]["details"]["reason"].s() == "cheating");
    CHECK(eventsOf(*socket, "action")[0]["action"]["id"].i() == actions[0]["id"].i());

    // Gone already
    CHECK(panel.post(std::format("/api/players/{}/kick", bobId)).status == 404);
}

TEST_CASE("A game ended from the panel sends its players back to the lobby", "[server][admin]")
{
    const rtype::test::TestDatabase test(rtype::database::Engine::Sqlite, "admin_end");
    AdminMaster master(test, rtype::network::ServerIdentity::generate());
    const Panel panel(master);
    const auto socket = panel.events();
    auto alice = master.player("Alice");
    alice->send(lobby::message_type::CreateGame, lobby::encode(lobby::CreateGameMessage{"Doomed", 2}));
    const lobby::GameJoinedMessage game = master.joined(*alice, 1);
    GameClient client(master.loop, game);
    master.gameClients.push_back(&client);
    REQUIRE(master.wait([&] { return client.accepted; }));

    CHECK(panel.post(std::format("/api/games/{}/end", game.gameId), R"({"reason":"maintenance"})").status ==
          200);

    REQUIRE(master.wait([&] { return alice->received.size() > 2 && client.end.has_value(); }));
    CHECK(lobby::decodeLeftGame(alice->received[2].second) ==
          lobby::LeftGameMessage{game.gameId, lobby::LeftGameReason::EndedByAdmin});
    CHECK(client.end->reason == rtype::network::DisconnectReason::Shutdown);
    REQUIRE(master.wait([&] { return master.games.empty(); }));
    CHECK_FALSE(alice->closed);
    REQUIRE(master.wait([&] { return !eventsOf(*socket, "game.ended").empty(); }));
    CHECK(eventsOf(*socket, "game.ended")[0]["reason"].s() == "ended_by_admin");
    CHECK(panel.post(std::format("/api/games/{}/end", game.gameId)).status == 404);
}

TEST_CASE("A message from the panel reaches a game, or every player", "[server][admin]")
{
    const rtype::test::TestDatabase test(rtype::database::Engine::Sqlite, "admin_message");
    AdminMaster master(test, rtype::network::ServerIdentity::generate());
    const Panel panel(master);
    auto alice = master.player("Alice");
    auto bob = master.player("Bob");
    alice->send(lobby::message_type::CreateGame, lobby::encode(lobby::CreateGameMessage{"Duo", 2}));
    const lobby::GameJoinedMessage game = master.joined(*alice, 1);

    const HttpResponse toGame =
        panel.post("/api/messages", std::format(R"({{"text":"Last wave","gameId":{}}})", game.gameId));
    CHECK(toGame.status == 200);
    CHECK(parse(toGame.body)["recipients"].i() == 1);
    const HttpResponse toAll =
        panel.post("/api/messages", R"({"text":"Restart in 5 minutes 🚀","gameId":null})");
    CHECK(parse(toAll.body)["recipients"].i() == 2);

    REQUIRE(master.wait([&] {
        return alice->count(lobby::message_type::ServerMessage) == 2 &&
               bob->count(lobby::message_type::ServerMessage) == 1;
    }));
    CHECK(lobby::decodeServerMessage(*bob->last(lobby::message_type::ServerMessage)) ==
          lobby::ServerMessageMessage{"Restart in 5 minutes 🚀"});
    CHECK(panel.post("/api/messages", R"({"text":"Hello","gameId":999})").status == 404);
}

TEST_CASE("A banned address is refused at HELLO, after a restart too, and welcome again once expired",
          "[server][admin]")
{
    const rtype::test::TestDatabase test(rtype::database::Engine::Sqlite, "admin_ban");
    const rtype::network::ServerIdentity identity = rtype::network::ServerIdentity::generate();
    const auto hello = [](AdminMaster &master) {
        testing::LobbyClient client(master.loop, master.lobby.localAddress(), master.identity.fingerprint());
        REQUIRE(waitUntil(master.loop, [&] { return client.connection.has_value(); }));
        client.hello("Mallory");
        const auto answer = client.reply(master.loop);
        REQUIRE(answer);
        if (answer->first == lobby::message_type::Error) {
            CHECK(lobby::decodeError(answer->second) ==
                  ErrorMessage{lobby::message_type::Hello, ErrorCode::Banned});
            CHECK(waitUntil(master.loop, [&] { return client.closed.has_value(); }));
            return false;
        }
        return answer->first == lobby::message_type::Welcome;
    };
    constexpr auto Duration = 4s;
    Clock::time_point expires;
    {
        AdminMaster master(test, identity);
        const Panel panel(master);
        auto alice = master.player("Alice");
        const HttpResponse banned =
            panel.post("/api/bans", std::format(R"({{"address":"127.0.0.1","duration":{},"reason":"flood"}})",
                                                Duration.count()));
        REQUIRE(banned.status == 200);
        const crow::json::rvalue ban = parse(banned.body);
        CHECK(ban["removed"].i() == 1);
        CHECK(ban["ban"]["address"].s() == "127.0.0.1");
        expires = fromUnixMilliseconds(ban["ban"]["expiresAt"].i());
        // Alice, connected from it, is removed
        REQUIRE(master.wait([&] { return alice->closed.has_value(); }));
        CHECK(lobby::decodeError(*alice->last(lobby::message_type::Error)) ==
              ErrorMessage{0, ErrorCode::Banned});
        CHECK_FALSE(hello(master));
        CHECK(parse(panel.get("/api/bans").body).size() == 1);
    }
    {
        // The next start reads it again
        AdminMaster master(test, identity);
        REQUIRE(Clock::now() < expires);
        CHECK_FALSE(hello(master));
    }
    std::this_thread::sleep_until(expires + 100ms);
    {
        AdminMaster master(test, identity);
        CHECK(hello(master));
        CHECK(master.administration->bans().empty());
    }
}

TEST_CASE("A ban lifted from the panel lets the address in again at once", "[server][admin]")
{
    const rtype::test::TestDatabase test(rtype::database::Engine::Sqlite, "admin_unban_http");
    AdminMaster master(test, rtype::network::ServerIdentity::generate());
    const Panel panel(master);
    const auto socket = panel.events();
    const auto welcomed = [&] {
        testing::LobbyClient client(master.loop, master.lobby.localAddress(), master.identity.fingerprint());
        REQUIRE(waitUntil(master.loop, [&] { return client.connection.has_value(); }));
        client.hello("Mallory");
        const auto answer = client.reply(master.loop);
        REQUIRE(answer);
        return answer->first == lobby::message_type::Welcome;
    };

    REQUIRE(panel.post("/api/bans", R"({"address":"127.0.0.1","duration":3600,"reason":"flood"})").status ==
            200);
    CHECK_FALSE(welcomed());

    const HttpResponse lifted =
        panel.post("/api/bans/lift", R"({"address":"127.0.0.1","reason":"a mistake"})");
    CHECK(lifted.status == 200);
    CHECK(parse(lifted.body)["address"].s() == "127.0.0.1");
    CHECK(welcomed());
    CHECK(parse(panel.get("/api/bans").body).size() == 0);
    CHECK(parse(panel.get("/api/server").body)["bans"].i() == 0);

    // Recorded, and told to the panel
    REQUIRE(master.wait([&] { return eventsOf(*socket, "action").size() == 2; }));
    const std::vector<crow::json::rvalue> actions = eventsOf(*socket, "action");
    const crow::json::rvalue &event = actions[1];
    CHECK(event["action"]["action"].s() == "unban");
    CHECK(event["action"]["target"].s() == "address:127.0.0.1");
    CHECK(event["action"]["details"]["reason"].s() == "a mistake");

    // Lifted already, never banned, or not an address
    CHECK(panel.post("/api/bans/lift", R"({"address":"127.0.0.1"})").status == 404);
    CHECK(panel.post("/api/bans/lift", R"({"address":"203.0.113.9"})").status == 404);
    CHECK(panel.post("/api/bans/lift", R"({"address":"nowhere"})").status == 400);
    CHECK(panel.post("/api/bans/lift", R"({"reason":"no address"})").status == 400);
}

TEST_CASE("The API refuses a visitor without a session, a change without its CSRF token, and wrong JSON",
          "[server][admin]")
{
    const rtype::test::TestDatabase test(rtype::database::Engine::Sqlite, "admin_refusals");
    AdminMaster master(test, rtype::network::ServerIdentity::generate());
    const Panel panel(master);
    auto alice = master.player("Alice");

    // No session
    for (const std::string path :
         {"/api/server", "/api/games", "/api/games/1", "/api/players", "/api/actions", "/api/bans"}) {
        INFO(path);
        CHECK(master.call(path).status == 401);
    }
    for (const std::string path :
         {"/api/players/1/kick", "/api/games/1/end", "/api/messages", "/api/bans", "/api/bans/lift"}) {
        INFO(path);
        CHECK(master.call(path, {.method = "POST", .headers = {}}, R"({"text":"x"})").status == 401);
    }
    // A session without its CSRF token, or with another one
    const HttpRequest withoutToken{.method = "POST", .headers = {"Cookie: " + panel.session}};
    CHECK(master.call("/api/players/1/kick", withoutToken).status == 403);
    CHECK(master.call("/api/bans/lift", withoutToken, R"({"address":"127.0.0.1"})").status == 403);
    const HttpRequest wrongToken{
        .method = "POST",
        .headers = {"Cookie: " + panel.session, "X-CSRF-Token: " + formatSecret(newAccessToken())}};
    CHECK(master.call("/api/players/1/kick", wrongToken).status == 403);
    const Panel other(master);
    const HttpRequest anotherSession{.method = "POST",
                                     .headers = {"Cookie: " + panel.session, "X-CSRF-Token: " + other.csrf}};
    CHECK(master.call("/api/players/1/kick", anotherSession).status == 403);
    // Alice is still there: nothing was done
    CHECK_FALSE(alice->closed);
    CHECK(parse(panel.get("/api/actions").body).size() == 0);

    // Wrong JSON
    for (const auto &[path, body] : std::vector<std::pair<std::string, std::string>>{
             {"/api/players/1/kick", "nonsense"},
             {"/api/players/1/kick", "[]"},
             {"/api/players/1/kick", R"({"reason":3})"},
             {"/api/players/1/kick", std::format(R"({{"reason":"{}"}})", std::string(201, 'r'))},
             {"/api/games/1/end", R"({"reason":"two\nlines"})"},
             {"/api/messages", ""},
             {"/api/messages", R"({"text":""})"},
             {"/api/messages", std::format(R"({{"text":"{}"}})", std::string(201, 'm'))},
             {"/api/messages", R"({"text":"Hi","gameId":"7"})"},
             {"/api/messages", R"({"text":"Hi","gameId":0})"},
             {"/api/bans", R"({"address":"nonsense","duration":60})"},
             {"/api/bans", R"({"address":"127.0.0.1"})"},
             {"/api/bans", R"({"address":"127.0.0.1","duration":0})"},
             {"/api/bans", R"({"address":"127.0.0.1","duration":604801})"},
             {"/api/bans", R"({"address":"127.0.0.1","duration":1.5})"},
         }) {
        INFO(path << " " << body);
        const HttpResponse response = panel.post(path, body);
        CHECK(response.status == 400);
        CHECK(response.header("content-type") == "application/json");
    }
    CHECK(panel.get("/api/actions?limit=0").status == 400);
    CHECK(panel.get("/api/actions?limit=501").status == 400);
    CHECK(panel.get("/api/actions?before=x").status == 400);
    CHECK(panel.get("/api/actions?limit=10&before=5").status == 200);
    CHECK_FALSE(alice->closed);
    CHECK(master.administration->bans().empty());

    // The WebSocket: a session, and a page of the panel
    const auto address = master.web->localAddress();
    const std::string origin = std::format("Origin: http://{}", address);
    for (const auto &[headers, status] : std::vector<std::pair<std::vector<std::string>, int>>{
             {{origin}, 401},
             {{"Cookie: " + panel.session, "Origin: http://evil.example.org"}, 403},
             {{"Cookie: " + panel.session, std::format("Origin: http://{}.evil.example.org", address.ip())},
              403},
             {{"Cookie: " + panel.session}, 403},
             {{"Cookie: " + panel.session, origin}, 101},
         }) {
        testing::WebSocketClient socket(master.loop, address, headers);
        REQUIRE(master.wait([&] { return socket.status.has_value() || socket.closed; }));
        CHECK(socket.status == status);
    }
}
