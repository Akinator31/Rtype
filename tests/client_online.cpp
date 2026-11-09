// Tests of the client online: its LobbyClient and its RemoteSession against a real master (the Lobby and the
// Games of r-type_server, with real instances), all on the loop of the test
// (docs/architecture/client-online.md). The accounts against a master with accounts, a mailer of the tests
// that keeps the codes, a fake Discord, on every engine of the database.

#include "Accounts/AccountService.hpp"
#include "Accounts/Discord.hpp"
#include "Accounts/PasswordHasher.hpp"
#include "FakeDiscord.hpp"
#include "FakeMailer.hpp"
#include "FakeVoiceDevices.hpp"
#include "Game/RemoteSession.hpp"
#include "GameServers.hpp"
#include "LobbyClient.hpp"
#include "Master/Games.hpp"
#include "Master/Lobby.hpp"
#include "Master/ServerDatabase.hpp"
#include "Online/LobbyClient.hpp"
#include "Online/OnlineScenes.hpp"
#include "Online/SessionTokens.hpp"
#include "TestDatabase.hpp"
#include "rtype/database/DatabaseThread.hpp"
#include "rtype/engine/Settings/Settings.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/TcpListener.hpp"
#include "rtype/network/Tls.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <numbers>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

using namespace std::chrono_literals;
using rtype::client::LobbyClient;
using rtype::client::RemoteSession;
using rtype::client::VoiceDevices;
using rtype::network::Address;
using rtype::network::EventLoop;
using rtype::network::ServerIdentity;
using rtype::network::TcpListener;
using testing::waitUntil;

namespace lobby = rtype::lobby;
namespace game = rtype::game;
namespace voice = rtype::voice;

namespace
{

/// A master on the loopback: its games, run by real instances, and its lobby.
struct Master {
    EventLoop loop;
    ServerIdentity identity = ServerIdentity::generate();
    rtype::server::Relay relay{Address::loopbackIpv4(0)};
    rtype::server::Games games;
    rtype::server::Lobby lobby;

    explicit Master(rtype::server::GamesSettings settings = testing::instances())
        : games(loop, relay, std::move(settings)),
          lobby(loop, TcpListener(loop, Address::loopbackIpv4(0)), identity, games)
    {
    }
    Master(const Master &) = delete;
    Master &operator=(const Master &) = delete;

    ~Master()
    {
        games.shutdown();
        (void)waitUntil(loop, [&] { return games.empty(); });
    }
};

/// The last code of an ERROR (lobby protocol 3).
constexpr std::uint16_t LastErrorCode = static_cast<std::uint16_t>(lobby::ErrorCode::DiscordUnavailable);

/// The password of the accounts of the tests.
const std::string Password = "correct horse battery";

/// A master with accounts on the loopback: its database of its own, cheap hashes, a mailer that keeps the
/// codes, and Discord through a fake one if asked. Its games are not played.
struct AccountsMaster {
    EventLoop loop;
    rtype::test::TestDatabase test;
    ServerIdentity identity = ServerIdentity::generate();
    rtype::server::Relay relay{Address::loopbackIpv4(0)};
    rtype::server::Games games{loop, relay,
                               rtype::server::GamesSettings{.executable = RTYPE_TEST_CHILD,
                                                            .leadingArguments = {},
                                                            .trailingArguments = {}}};
    testing::FakeMailer mailer;
    std::optional<testing::FakeDiscord> fakeDiscord;
    std::optional<rtype::database::DatabaseThread> database;
    std::optional<rtype::server::PasswordHasher> hasher;
    std::optional<rtype::server::DiscordClient> discord;
    std::optional<rtype::server::AccountService> accounts;
    std::optional<rtype::server::Lobby> lobby;

    AccountsMaster(rtype::database::Engine engine, std::string_view name, bool withDiscord = false)
        : test(engine, name)
    {
        test.open().migrate(rtype::server::ServerMigrations);
        const auto post = [this](std::function<void()> function) { loop.post(std::move(function)); };
        database.emplace(test.open(), post);
        hasher.emplace(
            post, rtype::server::PasswordHasher::Settings{
                      .threads = 2, .maxPending = 32, .parameters = {.memory = 64, .passes = 1, .lanes = 1}});
        if (withDiscord) {
            fakeDiscord.emplace();
            discord.emplace(fakeDiscord->settings(), post);
        }
        // Limits per address far away: every client of the tests comes from the loopback
        accounts.emplace(loop, *database, *hasher, mailer,
                         rtype::server::AccountServiceSettings{.guests = true,
                                                               .limits = {.requestsPerAddress = 1000,
                                                                          .addressWindow = 1min,
                                                                          .wrongPasswords = 10,
                                                                          .wrongPasswordPause = 15min,
                                                                          .resendInterval = 1min,
                                                                          .purgeInterval = 1h}},
                         discord ? &*discord : nullptr);
        lobby.emplace(loop, TcpListener(loop, Address::loopbackIpv4(0)), identity, games);
        lobby->setAccounts(&*accounts);
    }

    ~AccountsMaster()
    {
        lobby.reset();
        accounts.reset();
        discord.reset();
        hasher.reset();
        database.reset();
    }

    AccountsMaster(const AccountsMaster &) = delete;
    AccountsMaster &operator=(const AccountsMaster &) = delete;

    /// A client of this master, its key pinned.
    std::unique_ptr<LobbyClient> client(const std::string &name, LobbyClient::Handlers handlers,
                                        std::optional<lobby::SessionToken> token = std::nullopt)
    {
        return std::make_unique<LobbyClient>(loop, lobby->localAddress(), std::vector{identity.fingerprint()},
                                             name, std::move(handlers), "en", token);
    }
};

/// Settings in a file of their own, removed after the test: a directory per test, since CTest runs them at
/// once.
struct TemporarySettings {
    std::filesystem::path path;
    rtype::engine::Settings settings;

    explicit TemporarySettings(const std::string &test)
        : path(std::filesystem::temp_directory_path() / ("rtype_tests_client_online_" + test) /
               "settings.txt"),
          settings(path)
    {
        std::filesystem::remove_all(path.parent_path());
    }
    ~TemporarySettings() { std::filesystem::remove_all(path.parent_path()); }
};

/// What a LobbyClient told.
struct Events {
    std::optional<std::uint32_t> welcomed;
    std::vector<lobby::LeftGameMessage> left;
    std::vector<std::string> messages;
    std::optional<std::string> closed;

    LobbyClient::Handlers handlers()
    {
        return {
            .onWelcome = [this](std::uint32_t player) { welcomed = player; },
            .onLeftGame = [this](const lobby::LeftGameMessage &message) { left.push_back(message); },
            .onServerMessage = [this](const std::string &text) { messages.push_back(text); },
            .onClosed = [this](const std::string &reason) { closed = reason; },
            .onUntrusted = {},
            .onSessionExpired = {},
            .describe = {},
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
    std::set<std::string_view> keys;
    for (std::uint16_t code = 1; code <= LastErrorCode; ++code) {
        const std::string message = rtype::client::describe(static_cast<lobby::ErrorCode>(code));
        CHECK_FALSE(message.empty());
        CHECK_FALSE(message.starts_with("The server refused (error"));
        messages.insert(message);
        const std::string_view key = rtype::client::errorKey(static_cast<lobby::ErrorCode>(code));
        CHECK(key != "error.unknown");
        keys.insert(key);
    }
    // HELLO_REQUIRED and ALREADY_WELCOMED share theirs: the player can do nothing about either
    CHECK(messages.size() == LastErrorCode - 1);
    CHECK(keys.size() == LastErrorCode);
    CHECK_FALSE(rtype::client::describe(static_cast<lobby::ErrorCode>(999)).empty());
    CHECK(rtype::client::errorKey(static_cast<lobby::ErrorCode>(999)) == "error.unknown");
}

TEST_CASE("Every error of the master has a message in English and in French", "[online]")
{
    const std::filesystem::path locales = RTYPE_TEST_LOCALES;
    for (const char *language : {"en", "fr"}) {
        CAPTURE(language);
        rtype::localization::Localization translations(language);
        translations.load(language, locales / (std::string(language) + ".lang"));
        for (std::uint16_t code = 1; code <= LastErrorCode + 1; ++code) {
            const std::string_view key = rtype::client::errorKey(static_cast<lobby::ErrorCode>(code));
            CAPTURE(key);
            // A key missing from the catalog is shown as the key itself
            CHECK(translations.text(key) != key);
        }
    }
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

TEST_CASE("Two clients online hear each other through the game once both have a voice", "[online][voice]")
{
    Master master;
    Events events;
    LobbyClient alice(master.loop, master.lobby.localAddress(), {master.identity.fingerprint()}, "Alice",
                      events.handlers());
    Events bobEvents;
    LobbyClient bob(master.loop, master.lobby.localAddress(), {master.identity.fingerprint()}, "Bob",
                    bobEvents.handlers());
    REQUIRE(waitUntil(master.loop, [&] { return alice.welcomed() && bob.welcomed(); }));
    const lobby::GameJoinedMessage aliceGame = create(master.loop, alice, "Voices");
    const auto bobJoined = answer<LobbyClient::JoinResult>(
        master.loop, [&](auto done) { bob.joinGame(aliceGame.gameId, std::move(done)); });
    REQUIRE(bobJoined);

    RemoteSession aliceSession(master.loop, Address::loopbackIpv4(aliceGame.udpPort), aliceGame);
    RemoteSession bobSession(master.loop, Address::loopbackIpv4(aliceGame.udpPort),
                             std::get<lobby::GameJoinedMessage>(*bobJoined));
    const std::vector<RemoteSession *> both = {&aliceSession, &bobSession};
    // Before the game accepted it, the backend is kept and told at the acceptance
    aliceSession.setVoiceBackend(voice::Backend::Game);
    const game::VoiceMessage hello{1, false, {std::byte{0xF8}, std::byte{0x01}}};
    CHECK_FALSE(aliceSession.sendVoice(hello));
    REQUIRE(play(master.loop, both, [&] { return aliceSession.accepted() && bobSession.accepted(); }));

    // Bob has no voice yet: nothing is sent to him. A few frames of time, so that the game has it before Bob
    // says he has a voice
    CHECK(aliceSession.sendVoice({0, false, hello.frame}));
    CHECK_FALSE(bobSession.sendVoice(hello));
    int frames = 0;
    REQUIRE(play(master.loop, both, [&] { return ++frames > 12; }));
    bobSession.setVoiceBackend(voice::Backend::Game);
    const game::VoiceRosterMessage roster{{voice::Backend::Game, voice::Backend::Game}};
    REQUIRE(play(master.loop, both,
                 [&] { return aliceSession.voiceRoster() == roster && bobSession.voiceRoster() == roster; }));
    CHECK(bobSession.takeVoiceFrames().empty());

    // Now Alice talks, and Bob receives her frame, with her slot
    CHECK(aliceSession.sendVoice(hello));
    std::vector<game::VoiceFrameMessage> heard;
    REQUIRE(play(master.loop, both, [&] {
        for (game::VoiceFrameMessage &frame : bobSession.takeVoiceFrames()) {
            heard.push_back(std::move(frame));
        }
        return !heard.empty();
    }));
    REQUIRE(heard.size() == 1);
    CHECK(heard[0].slot == 0);
    CHECK(heard[0].sequence == 1);
    CHECK(heard[0].frame == hello.frame);
    // She never hears herself
    CHECK(aliceSession.takeVoiceFrames().empty());
}

TEST_CASE("The microphone of a client online is heard from the stream of its slot on the other",
          "[online][voice]")
{
    Master master;
    Events events;
    LobbyClient alice(master.loop, master.lobby.localAddress(), {master.identity.fingerprint()}, "Alice",
                      events.handlers());
    Events bobEvents;
    LobbyClient bob(master.loop, master.lobby.localAddress(), {master.identity.fingerprint()}, "Bob",
                    bobEvents.handlers());
    REQUIRE(waitUntil(master.loop, [&] { return alice.welcomed() && bob.welcomed(); }));
    const lobby::GameJoinedMessage aliceGame = create(master.loop, alice, "Voices");
    const auto bobJoined = answer<LobbyClient::JoinResult>(
        master.loop, [&](auto done) { bob.joinGame(aliceGame.gameId, std::move(done)); });
    REQUIRE(bobJoined);

    testing::FakeMicrophone aliceMicrophone;
    testing::FakePlayback alicePlayback;
    VoiceDevices aliceVoice{.capture = aliceMicrophone, .playback = alicePlayback};
    testing::FakeMicrophone bobMicrophone;
    testing::FakePlayback bobPlayback;
    VoiceDevices bobVoice{.capture = bobMicrophone, .playback = bobPlayback};
    RemoteSession aliceSession(master.loop, Address::loopbackIpv4(aliceGame.udpPort), aliceGame, {},
                               &aliceVoice);
    RemoteSession bobSession(master.loop, Address::loopbackIpv4(aliceGame.udpPort),
                             std::get<lobby::GameJoinedMessage>(*bobJoined), {}, &bobVoice);
    const std::vector<RemoteSession *> both = {&aliceSession, &bobSession};
    // Both on the voice of the game once accepted, each knowing the other
    const game::VoiceRosterMessage roster{{voice::Backend::Game, voice::Backend::Game}};
    REQUIRE(play(master.loop, both, [&] {
        return aliceSession.voiceRoster() == roster && bobSession.voiceRoster() == roster &&
               aliceSession.voice()->participants().size() == 1 &&
               bobSession.voice()->participants().size() == 1;
    }));
    CHECK(bobPlayback.streams[0]);
    // In push-to-talk by default: the microphone is open, and nothing is sent until the key is held
    CHECK(aliceMicrophone.isOpen);
    CHECK(aliceSession.voice()->sentFrames() == 0);

    // Alice holds the key and talks: a frame of her voice every 20 ms
    aliceVoice.pushToTalk = true;
    double phase = 0.0;
    auto nextFrame = std::chrono::steady_clock::now();
    REQUIRE(play(master.loop, both, [&] {
        if (std::chrono::steady_clock::now() >= nextFrame) {
            nextFrame += std::chrono::milliseconds(20);
            std::array<std::int16_t, voice::FrameSamples> frame{};
            for (std::int16_t &sample : frame) {
                phase += 2.0 * std::numbers::pi * 180.0 / voice::SampleRate;
                sample = static_cast<std::int16_t>(std::sin(phase) * 6000.0 + std::sin(3.0 * phase) * 2000.0);
            }
            aliceMicrophone.say(frame);
        }
        return bobPlayback.played[0].size() >= 10 * voice::FrameSamples;
    }));
    CHECK(aliceMicrophone.isOpen);
    CHECK(bobSession.voice()->participants().at(0).speaking);
    // Something heard, not silence
    double sum = 0.0;
    for (const std::int16_t sample : bobPlayback.played[0]) {
        sum += static_cast<double>(sample) * sample;
    }
    CHECK(std::sqrt(sum / static_cast<double>(bobPlayback.played[0].size())) > 500.0);
    // Bob says nothing, and Alice never hears herself
    CHECK(alicePlayback.played[0].empty());
    CHECK(alicePlayback.played[1].empty());

    // She mutes herself with its key: closed, and Bob is told
    aliceVoice.pushToTalk = false;
    aliceVoice.muted = true;
    REQUIRE(play(master.loop, both, [&] { return !aliceMicrophone.isOpen; }));
    REQUIRE(play(master.loop, both, [&] { return bobSession.voice()->participants().at(0).status.muted; }));
    CHECK(bobSession.voiceStatuses().statuses[0] == voice::Status{.muted = true});
    CHECK(bobSession.voiceView()->local == rtype::client::LocalVoice{});
    CHECK(aliceSession.voiceView()->local.muted);

    // Bob turns Alice down, then mutes her, from his pause menu: kept under her name, a guest
    const std::optional<std::string> key = bobSession.voiceKey(0);
    REQUIRE(key);
    CHECK(*key == rtype::client::playerVoiceKey("", 0, "Alice"));
    CHECK(bobSession.accountOf(0) == 0u);
    bobVoice.preferences.players[*key] = {.volume = 0.5f, .muted = false};
    REQUIRE(play(master.loop, both, [&] { return bobSession.voice()->participants().at(0).volume == 0.5f; }));
    bobVoice.preferences.players[*key].muted = true;
    REQUIRE(play(master.loop, both, [&] { return bobSession.voice()->participants().at(0).mutedByMe; }));
    CHECK(bobPlayback.volume[0] == 0.0f);
}

TEST_CASE("A client online muted by an administrator closes its microphone and is shown muted to all",
          "[online][voice]")
{
    Master master;
    Events events;
    LobbyClient alice(master.loop, master.lobby.localAddress(), {master.identity.fingerprint()}, "Alice",
                      events.handlers());
    Events bobEvents;
    LobbyClient bob(master.loop, master.lobby.localAddress(), {master.identity.fingerprint()}, "Bob",
                    bobEvents.handlers());
    REQUIRE(waitUntil(master.loop, [&] { return alice.welcomed() && bob.welcomed(); }));
    const lobby::GameJoinedMessage aliceGame = create(master.loop, alice, "Voices");
    const auto bobJoined = answer<LobbyClient::JoinResult>(
        master.loop, [&](auto done) { bob.joinGame(aliceGame.gameId, std::move(done)); });
    REQUIRE(bobJoined);

    testing::FakeMicrophone aliceMicrophone;
    testing::FakePlayback alicePlayback;
    VoiceDevices aliceVoice{.capture = aliceMicrophone, .playback = alicePlayback};
    testing::FakeMicrophone bobMicrophone;
    testing::FakePlayback bobPlayback;
    VoiceDevices bobVoice{.capture = bobMicrophone, .playback = bobPlayback};
    RemoteSession aliceSession(master.loop, Address::loopbackIpv4(aliceGame.udpPort), aliceGame, {},
                               &aliceVoice);
    RemoteSession bobSession(master.loop, Address::loopbackIpv4(aliceGame.udpPort),
                             std::get<lobby::GameJoinedMessage>(*bobJoined), {}, &bobVoice);
    const std::vector<RemoteSession *> both = {&aliceSession, &bobSession};
    REQUIRE(play(master.loop, both, [&] {
        return aliceSession.voice()->joined() && bobSession.voice()->participants().size() == 1;
    }));
    REQUIRE(aliceMicrophone.isOpen);

    // An administrator mutes Alice: her microphone closes, her HUD and the one of Bob show it
    REQUIRE(master.games.muteVoice(*alice.playerId(), true));
    REQUIRE(play(master.loop, both, [&] {
        return aliceSession.mutedByAdmin() && bobSession.voice()->participants().at(0).status.mutedByAdmin;
    }));
    REQUIRE(play(master.loop, both, [&] { return !aliceMicrophone.isOpen; }));
    CHECK(aliceSession.voiceView()->local.mutedByAdmin);
    CHECK(rtype::client::voiceIconOf(bobSession.voiceView()->others.at(0)) ==
          rtype::client::VoiceIcon::MutedByAdmin);

    // Let talk again: her microphone opens again
    REQUIRE(master.games.muteVoice(*alice.playerId(), false));
    REQUIRE(play(master.loop, both, [&] { return !aliceSession.mutedByAdmin() && aliceMicrophone.isOpen; }));
    REQUIRE(play(master.loop, both,
                 [&] { return !bobSession.voice()->participants().at(0).status.mutedByAdmin; }));
}

TEST_CASE("A client online in a game without voice opens no microphone, shows no voice and sends none",
          "[online][voice]")
{
    rtype::server::GamesSettings settings = testing::instances();
    settings.voice.enabled = false;
    Master master(settings);
    Events events;
    LobbyClient alice(master.loop, master.lobby.localAddress(), {master.identity.fingerprint()}, "Alice",
                      events.handlers());
    REQUIRE(waitUntil(master.loop, [&] { return alice.welcomed(); }));
    const lobby::GameJoinedMessage game = create(master.loop, alice, "Quiet");
    testing::FakeMicrophone microphone;
    testing::FakePlayback playback;
    VoiceDevices devices{.capture = microphone, .playback = playback};
    devices.pushToTalk = true;
    RemoteSession session(master.loop, Address::loopbackIpv4(game.udpPort), game, {}, &devices);
    const std::vector<RemoteSession *> one = {&session};
    REQUIRE(play(master.loop, one, [&] { return session.accepted(); }));
    int frames = 0;
    REQUIRE(play(master.loop, one, [&] { return ++frames > 12; }));

    CHECK_FALSE(session.voice()->joined());
    CHECK(microphone.opens == 0);
    CHECK_FALSE(session.voiceView());
    CHECK_FALSE(session.sendVoice({0, false, {std::byte{0xF8}}}));
    CHECK(session.voiceRoster() == game::VoiceRosterMessage{});
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

TEST_CASE("A client is told what the administrators do: a message, a game ended, a removal", "[online]")
{
    Master master;
    Events events;
    LobbyClient alice(master.loop, master.lobby.localAddress(), {master.identity.fingerprint()}, "Alice",
                      events.handlers());
    REQUIRE(waitUntil(master.loop, [&] { return alice.welcomed(); }));

    // A message: a notification, between the replies
    REQUIRE(master.lobby.sendMessage(*alice.playerId(), "Restart in 5 minutes"));
    REQUIRE(waitUntil(master.loop, [&] { return !events.messages.empty(); }));
    CHECK(events.messages == std::vector<std::string>{"Restart in 5 minutes"});
    CHECK(list(master.loop, alice).empty());

    // A game ended: back in the lobby
    const lobby::GameJoinedMessage game = create(master.loop, alice, "Doomed");
    REQUIRE(master.games.end(game.gameId));
    REQUIRE(waitUntil(master.loop, [&] { return !events.left.empty(); }));
    CHECK(events.left[0] == lobby::LeftGameMessage{game.gameId, lobby::LeftGameReason::EndedByAdmin});

    // Removed: out of its game, then of the server, with the reason
    const lobby::GameJoinedMessage second = create(master.loop, alice, "Second");
    REQUIRE(master.games.kick(*alice.playerId()));
    REQUIRE(master.lobby.disconnect(*alice.playerId(), lobby::ErrorCode::Kicked));
    REQUIRE(waitUntil(master.loop, [&] { return events.closed.has_value(); }));
    CHECK(events.left.back() == lobby::LeftGameMessage{second.gameId, lobby::LeftGameReason::Kicked});
    CHECK(*events.closed == rtype::client::describe(lobby::ErrorCode::Kicked));
    CHECK_FALSE(alice.welcomed());
}

// --- The accounts ---

namespace
{

#define FOR_EACH_ENGINE()                                                                                    \
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));                                    \
    CAPTURE(rtype::test::engineName(engine))

/// Creates an account from a client welcomed as a guest, and verifies it with the code of its email.
lobby::SignedInMessage createAccount(AccountsMaster &master, LobbyClient &client, const std::string &username,
                                     const std::string &email)
{
    std::optional<std::optional<lobby::ErrorCode>> registered;
    client.registerAccount(username, email, Password, [&](auto error) { registered = error; });
    REQUIRE(waitUntil(master.loop, [&] { return registered.has_value(); }));
    REQUIRE_FALSE(*registered);
    const std::optional<std::string> code = master.mailer.code(email);
    REQUIRE(code);

    const auto verified = answer<LobbyClient::SignInResult>(
        master.loop, [&](auto done) { client.verifyEmail(username, *code, std::move(done)); });
    REQUIRE(verified);
    REQUIRE(std::holds_alternative<lobby::SignedInMessage>(*verified));
    return std::get<lobby::SignedInMessage>(*verified);
}

} // namespace

TEST_CASE("A player creates an account, verifies it, and comes back signed in after a restart",
          "[online][accounts]")
{
    FOR_EACH_ENGINE();
    AccountsMaster master(engine, "client_accounts_create");
    TemporarySettings file("create");
    const std::string address =
        rtype::client::serverAddress("127.0.0.1", master.lobby->localAddress().port());

    {
        Events events;
        const auto guest = master.client("Pilot", events.handlers());
        REQUIRE(waitUntil(master.loop, [&] { return guest->welcomed(); }));
        // The client offers what the master offers
        CHECK(guest->features() ==
              lobby::ServerFeatures{.guests = true, .passwordAccounts = true, .discord = false});
        CHECK_FALSE(guest->account());

        const lobby::SignedInMessage signedIn = createAccount(master, *guest, "Pilot_7", "pilot@example.org");
        CHECK(signedIn.account.username == "Pilot_7");
        REQUIRE(guest->account());
        CHECK(guest->account()->username == "Pilot_7");
        CHECK(guest->name() == "Pilot_7");
        rtype::client::SessionTokens sessions(file.settings);
        CHECK(sessions.keep(address, signedIn.token));
        CHECK_FALSE(events.closed);
    }

    // The game restarts: its settings read again, the token sent in HELLO, without a name
    rtype::engine::Settings next(file.path);
    next.readFile();
    const std::optional<lobby::SessionToken> token = rtype::client::SessionTokens(next).token(address);
    REQUIRE(token);
    Events events;
    const auto back = master.client("", events.handlers(), token);
    REQUIRE(waitUntil(master.loop, [&] { return back->welcomed() || events.closed.has_value(); }));
    REQUIRE(back->account());
    CHECK(back->account()->username == "Pilot_7");
    CHECK(back->name() == "Pilot_7");
    // Signed in, it plays as its account
    CHECK(list(master.loop, *back).empty());
}

TEST_CASE("An expired token is told to the client, which forgets it and connects again as a guest",
          "[online][accounts]")
{
    FOR_EACH_ENGINE();
    AccountsMaster master(engine, "client_accounts_expired");
    TemporarySettings file("expired");
    rtype::client::SessionTokens sessions(file.settings);
    const std::string address =
        rtype::client::serverAddress("127.0.0.1", master.lobby->localAddress().port());
    lobby::SessionToken unknown{};
    unknown.fill(std::byte{0x5A});
    REQUIRE(sessions.keep(address, unknown));

    Events events;
    bool expired = false;
    LobbyClient::Handlers handlers = events.handlers();
    handlers.onSessionExpired = [&] {
        expired = true;
        sessions.forget(address);
    };
    const auto client = master.client("Pilot", std::move(handlers), sessions.token(address));
    REQUIRE(waitUntil(master.loop, [&] { return expired || events.closed.has_value(); }));
    CHECK(expired);
    // Told as an expired session, not as a connection lost
    CHECK_FALSE(events.closed);
    CHECK_FALSE(client->welcomed());
    CHECK_FALSE(sessions.token(address));

    Events again;
    const auto guest = master.client("Pilot", again.handlers(), sessions.token(address));
    REQUIRE(waitUntil(master.loop, [&] { return guest->welcomed() || again.closed.has_value(); }));
    CHECK(guest->welcomed());
    CHECK_FALSE(guest->account());
}

TEST_CASE("A player signs out: the connection closes without an error, and its token is refused after",
          "[online][accounts]")
{
    FOR_EACH_ENGINE();
    AccountsMaster master(engine, "client_accounts_sign_out");
    Events events;
    const auto client = master.client("Pilot", events.handlers());
    REQUIRE(waitUntil(master.loop, [&] { return client->welcomed(); }));
    const lobby::SignedInMessage signedIn = createAccount(master, *client, "Pilot_7", "pilot@example.org");

    std::optional<std::optional<lobby::ErrorCode>> done;
    client->signOut([&](auto error) { done = error; });
    REQUIRE(waitUntil(master.loop, [&] { return done.has_value(); }));
    CHECK_FALSE(*done);
    CHECK_FALSE(client->welcomed());
    CHECK_FALSE(client->account());
    // The master closed the connection after DONE: not reported as a connection lost
    testing::runFor(master.loop, 200ms);
    CHECK_FALSE(events.closed);

    Events back;
    bool expired = false;
    LobbyClient::Handlers handlers = back.handlers();
    handlers.onSessionExpired = [&] { expired = true; };
    const auto again = master.client("", std::move(handlers), signedIn.token);
    REQUIRE(waitUntil(master.loop, [&] { return expired || back.closed.has_value(); }));
    CHECK(expired);
}

TEST_CASE("A wrong password gets its message, and fields the protocol cannot carry are refused at once",
          "[online][accounts]")
{
    FOR_EACH_ENGINE();
    AccountsMaster master(engine, "client_accounts_wrong_password");
    Events events;
    const auto client = master.client("Pilot", events.handlers());
    REQUIRE(waitUntil(master.loop, [&] { return client->welcomed(); }));
    {
        Events creatorEvents;
        const auto creator = master.client("Creator", creatorEvents.handlers());
        REQUIRE(waitUntil(master.loop, [&] { return creator->welcomed(); }));
        (void)createAccount(master, *creator, "Pilot_7", "pilot@example.org");
    }

    const auto wrong = answer<LobbyClient::SignInResult>(
        master.loop, [&](auto done) { client->signIn("Pilot_7", "not the password", std::move(done)); });
    REQUIRE(wrong);
    REQUIRE(std::holds_alternative<lobby::ErrorCode>(*wrong));
    CHECK(std::get<lobby::ErrorCode>(*wrong) == lobby::ErrorCode::InvalidCredentials);
    CHECK(rtype::client::describe(lobby::ErrorCode::InvalidCredentials) ==
          "Wrong username, email or password");
    CHECK(rtype::client::errorKey(lobby::ErrorCode::InvalidCredentials) == "error.invalid_credentials");
    CHECK_FALSE(client->account());

    // Answered at once, nothing sent
    std::optional<LobbyClient::SignInResult> empty;
    client->signIn("Pilot_7", "", [&](const auto &result) { empty = result; });
    REQUIRE(empty);
    CHECK(std::get<lobby::ErrorCode>(*empty) == lobby::ErrorCode::InvalidCredentials);
    std::optional<LobbyClient::SignInResult> code;
    client->verifyEmail("Pilot_7", "12a456", [&](const auto &result) { code = result; });
    REQUIRE(code);
    CHECK(std::get<lobby::ErrorCode>(*code) == lobby::ErrorCode::InvalidCode);
    std::optional<std::optional<lobby::ErrorCode>> username;
    client->registerAccount(std::string(17, 'a'), "a@example.org", Password,
                            [&](auto error) { username = error; });
    REQUIRE(username);
    CHECK(*username == lobby::ErrorCode::InvalidName);

    // The right password signs in
    const auto right = answer<LobbyClient::SignInResult>(
        master.loop, [&](auto done) { client->signIn("pilot@example.org", Password, std::move(done)); });
    REQUIRE(right);
    REQUIRE(std::holds_alternative<lobby::SignedInMessage>(*right));
    CHECK(client->account()->username == "Pilot_7");
    CHECK_FALSE(events.closed);
}

TEST_CASE("A forgotten password is changed with the code of its email, which signs in", "[online][accounts]")
{
    FOR_EACH_ENGINE();
    AccountsMaster master(engine, "client_accounts_reset");
    {
        Events creatorEvents;
        const auto creator = master.client("Creator", creatorEvents.handlers());
        REQUIRE(waitUntil(master.loop, [&] { return creator->welcomed(); }));
        (void)createAccount(master, *creator, "Pilot_7", "pilot@example.org");
    }
    Events events;
    const auto client = master.client("Pilot", events.handlers());
    REQUIRE(waitUntil(master.loop, [&] { return client->welcomed(); }));

    std::optional<std::optional<lobby::ErrorCode>> sent;
    client->forgotPassword("pilot@example.org", [&](auto error) { sent = error; });
    REQUIRE(waitUntil(master.loop, [&] { return sent.has_value(); }));
    CHECK_FALSE(*sent);
    const std::optional<std::string> code = master.mailer.code("pilot@example.org");
    REQUIRE(code);

    const auto reset = answer<LobbyClient::SignInResult>(master.loop, [&](auto done) {
        client->resetPassword("pilot@example.org", *code, "a brand new password", std::move(done));
    });
    REQUIRE(reset);
    REQUIRE(std::holds_alternative<lobby::SignedInMessage>(*reset));
    CHECK(client->account()->username == "Pilot_7");
}

TEST_CASE("A player deletes its account with its password, then the connection closes", "[online][accounts]")
{
    FOR_EACH_ENGINE();
    AccountsMaster master(engine, "client_accounts_delete");
    Events events;
    const auto client = master.client("Pilot", events.handlers());
    REQUIRE(waitUntil(master.loop, [&] { return client->welcomed(); }));
    (void)createAccount(master, *client, "Pilot_7", "pilot@example.org");

    std::optional<std::optional<lobby::ErrorCode>> wrong;
    client->deleteAccount("not the password", [&](auto error) { wrong = error; });
    REQUIRE(waitUntil(master.loop, [&] { return wrong.has_value(); }));
    CHECK(*wrong == lobby::ErrorCode::InvalidCredentials);
    CHECK(client->welcomed());

    std::optional<std::optional<lobby::ErrorCode>> deleted;
    client->deleteAccount(Password, [&](auto error) { deleted = error; });
    REQUIRE(waitUntil(master.loop, [&] { return deleted.has_value(); }));
    CHECK_FALSE(*deleted);
    CHECK_FALSE(client->welcomed());
    testing::runFor(master.loop, 200ms);
    CHECK_FALSE(events.closed);

    // The account is gone: its name is free for a guest
    Events guestEvents;
    const auto guest = master.client("Pilot_7", guestEvents.handlers());
    REQUIRE(waitUntil(master.loop, [&] { return guest->welcomed() || guestEvents.closed.has_value(); }));
    CHECK(guest->welcomed());
}

TEST_CASE("The session token of a server is kept in the settings, readable by the player only",
          "[online][accounts]")
{
    TemporarySettings file("tokens");
    rtype::client::SessionTokens sessions(file.settings);
    lobby::SessionToken token{};
    for (std::size_t i = 0; i < token.size(); ++i) {
        token[i] = static_cast<std::byte>(i * 7);
    }
    const std::string address = rtype::client::serverAddress("RType.example.org", 4242);
    CHECK_FALSE(sessions.token(address));

    CHECK(sessions.keep(address, token));
    CHECK(sessions.token(address) == token);
    CHECK_FALSE(sessions.token(rtype::client::serverAddress("other.example.org", 4242)));
    rtype::engine::Settings next(file.path);
    next.readFile();
    CHECK(next.getString("servers.session.rtype.example.org:4242").value_or("").size() == 64);
#ifndef _WIN32
    using std::filesystem::perms;
    CHECK(std::filesystem::status(file.path).permissions() == (perms::owner_read | perms::owner_write));
#endif

    // Anything but 64 hexadecimal digits is no token
    file.settings.setString("servers.session.rtype.example.org:4242", "not a token");
    CHECK_FALSE(sessions.token(address));

    CHECK(sessions.forget(address));
    CHECK_FALSE(sessions.forget(address));
    CHECK_FALSE(sessions.token(address));
}

TEST_CASE(
    "A player signs in with Discord: a new account, its username, then its deletion confirmed by Discord",
    "[online][accounts][discord]")
{
    FOR_EACH_ENGINE();
    AccountsMaster master(engine, "client_accounts_discord", true);
    const std::string png = std::string("\x89PNG\r\n\x1a\n", 8) + std::string(20000, 'p');
    const rtype::server::DiscordUser wing{.id = "80351110224678912",
                                          .username = "wing",
                                          .displayName = "Wing Commander",
                                          .avatarHash = "a1b2c3",
                                          .avatar = png};
    const auto confirmation = [](const rtype::server::DiscordAuthorization &accepted) {
        return lobby::DiscordConfirmMessage{.code = accepted.code,
                                            .codeVerifier = accepted.codeVerifier,
                                            .redirectUri = accepted.redirectUri};
    };
    Events events;
    const auto client = master.client("Pilot", events.handlers());
    REQUIRE(waitUntil(master.loop, [&] { return client->welcomed(); }));
    CHECK(client->features() ==
          lobby::ServerFeatures{.guests = true, .passwordAccounts = true, .discord = true});

    // The application of the master, for the Discord Social SDK, then what Discord gave
    const auto signInWithDiscord = [&] {
        std::optional<std::variant<lobby::DiscordAuthorizeMessage, lobby::ErrorCode>> authorize;
        client->discordSignIn([&](const auto &result) { authorize = result; });
        REQUIRE(waitUntil(master.loop, [&] { return authorize.has_value(); }));
        REQUIRE(std::holds_alternative<lobby::DiscordAuthorizeMessage>(*authorize));
        CHECK(std::to_string(std::get<lobby::DiscordAuthorizeMessage>(*authorize).applicationId) ==
              testing::FakeDiscord::ClientId);
        std::optional<LobbyClient::DiscordConfirmResult> confirmed;
        client->discordConfirm(confirmation(master.fakeDiscord->accept(wing)),
                               [&](const auto &result) { confirmed = result; });
        REQUIRE(waitUntil(master.loop, [&] { return confirmed.has_value(); }));
        return *confirmed;
    };

    // A new Discord user: not signed in until it chooses its username, its profile and picture shown
    // meanwhile
    const LobbyClient::DiscordConfirmResult newUser = signInWithDiscord();
    REQUIRE(std::holds_alternative<LobbyClient::DiscordConfirmed>(newUser));
    const auto &profile = std::get<LobbyClient::DiscordConfirmed>(newUser).newUser;
    REQUIRE(profile);
    CHECK(profile->displayName == "Wing Commander");
    CHECK(profile->discordUsername == "wing");
    CHECK(profile->avatarSize == png.size());
    std::optional<std::variant<LobbyClient::Picture, lobby::ErrorCode>> picture;
    client->picture(0, [&](const auto &result) { picture = result; });
    REQUIRE(waitUntil(master.loop, [&] { return picture.has_value(); }));
    REQUIRE(std::holds_alternative<LobbyClient::Picture>(*picture));
    CHECK(std::get<LobbyClient::Picture>(*picture).hash == "a1b2c3");
    CHECK(std::get<LobbyClient::Picture>(*picture).png.size() == png.size());
    CHECK_FALSE(client->account());
    std::optional<LobbyClient::SignInResult> created;
    client->chooseUsername("Wing", [&](const auto &result) { created = result; });
    REQUIRE(waitUntil(master.loop, [&] { return created.has_value(); }));
    REQUIRE(std::holds_alternative<lobby::SignedInMessage>(*created));
    REQUIRE(client->account());
    CHECK(client->account()->username == "Wing");
    CHECK(client->name() == "Wing");
    CHECK_FALSE(client->account()->hasPassword);
    std::optional<std::variant<lobby::ProfileMessage, lobby::ErrorCode>> kept;
    client->profile(client->account()->id, [&](const auto &result) { kept = result; });
    REQUIRE(waitUntil(master.loop, [&] { return kept.has_value(); }));
    REQUIRE(std::holds_alternative<lobby::ProfileMessage>(*kept));
    CHECK(std::get<lobby::ProfileMessage>(*kept).avatarHash == "a1b2c3");

    // What the protocol cannot carry is refused at once, without asking the master
    std::optional<LobbyClient::DiscordConfirmResult> malformed;
    client->discordConfirm(
        {.code = "code", .codeVerifier = "short", .redirectUri = "http://127.0.0.1/callback"},
        [&](const auto &result) { malformed = result; });
    REQUIRE(malformed);
    CHECK(std::get<lobby::ErrorCode>(*malformed) == lobby::ErrorCode::InvalidCode);

    // No password: Discord confirms the deletion
    std::optional<std::optional<lobby::ErrorCode>> refused;
    client->deleteAccount("", [&](auto error) { refused = error; });
    REQUIRE(waitUntil(master.loop, [&] { return refused.has_value(); }));
    CHECK(*refused == lobby::ErrorCode::DiscordNotConfirmed);
    CHECK(std::holds_alternative<LobbyClient::DiscordConfirmed>(signInWithDiscord()));
    std::optional<std::optional<lobby::ErrorCode>> deleted;
    client->deleteAccount("", [&](auto error) { deleted = error; });
    REQUIRE(waitUntil(master.loop, [&] { return deleted.has_value(); }));
    CHECK_FALSE(*deleted);
    CHECK_FALSE(client->welcomed());
    rtype::database::Database direct = master.test.open();
    CHECK_FALSE(rtype::server::findAccountByDiscord(direct, wing.id));
}
