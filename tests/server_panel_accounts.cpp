// Tests of the administrators who sign in to the panel with their account
// (docs/architecture/web.md#accounts): a real WebServer with its PanelAccounts and its Administration on the
// loop of the test, asked over HTTP with libcurl, a fake Discord, the games and the lobby faked, on every
// engine of the database. Who gets in, the sign-in with Discord and its Lax cookie, the limits, the actions
// on the accounts.

#include "Accounts/AccountRules.hpp"
#include "Accounts/AccountStore.hpp"
#include "Accounts/Discord.hpp"
#include "Accounts/PanelAccounts.hpp"
#include "Accounts/PasswordHasher.hpp"
#include "Common/Secret.hpp"
#include "FakeDiscord.hpp"
#include "Http.hpp"
#include "LobbyClient.hpp"
#include "Master/AddressBans.hpp"
#include "Master/AdminStore.hpp"
#include "Master/Administration.hpp"
#include "Master/ServerDatabase.hpp"
#include "TestDatabase.hpp"
#include "Web/LocalAccess.hpp"
#include "Web/WebServer.hpp"
#include "WebSocketClient.hpp"
#include "rtype/database/Database.hpp"
#include "rtype/database/DatabaseThread.hpp"
#include "rtype/log/Log.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <crow/json.h>
#include <crow/query_string.h>

#include <chrono>
#include <format>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

using namespace rtype::server;
using namespace std::chrono_literals;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::StartsWith;
using rtype::database::Database;
using rtype::database::DatabaseThread;
using rtype::lobby::ErrorCode;
using rtype::network::Address;
using rtype::network::EventLoop;
using rtype::test::TestDatabase;
using testing::FakeDiscord;
using testing::HttpRequest;
using testing::HttpResponse;
using testing::waitUntil;

namespace
{

/// Cheap hashes: these tests do not test Argon2.
constexpr Argon2Parameters CheapArgon2{.memory = 64, .passes = 1, .lanes = 1};
const std::string Password = "correct horse battery";
constexpr std::string_view PublicUrl = "https://admin.example.org";

#define FOR_EACH_ENGINE()                                                                                    \
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));                                    \
    CAPTURE(rtype::test::engineName(engine))

// --- Fakes of the games and the lobby ---

struct FakeGames final : AdminGames {
    std::vector<std::uint32_t> kicked;

    [[nodiscard]] std::vector<AdminGame> adminGames() const override { return {}; }
    bool kick(std::uint32_t playerId) override
    {
        kicked.push_back(playerId);
        return false;
    }
    bool end(std::uint32_t) override { return false; }
};

struct FakeLobby final : AdminLobby {
    std::vector<AdminPlayer> players;
    std::vector<std::pair<std::uint32_t, ErrorCode>> disconnected;

    [[nodiscard]] std::vector<AdminPlayer> adminPlayers() const override { return players; }
    bool disconnect(std::uint32_t playerId, ErrorCode code) override
    {
        if (std::erase_if(players, [&](const AdminPlayer &player) { return player.id == playerId; }) == 0) {
            return false;
        }
        disconnected.emplace_back(playerId, code);
        return true;
    }
    bool sendMessage(std::uint32_t, const std::string &) override { return true; }
};

/// Captures the messages logged during a test, from every thread, at every level.
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

crow::json::rvalue parse(const std::string &text)
{
    crow::json::rvalue json = crow::json::load(text);
    INFO(text);
    REQUIRE(json);
    return json;
}

/// What a test changes of the panel.
struct Setup {
    static Setup with(const std::function<void(Setup &)> &change)
    {
        Setup setup;
        change(setup);
        return setup;
    }

    std::vector<std::string> admins;
    /// Empty: no Discord for the panel.
    std::string publicUrl;
    std::vector<Subnet> trustedProxies;
    std::size_t signInsPerAddress = 1000;
    unsigned wrongPasswords = 10;
};

/// The web server of a master with accounts, its PanelAccounts and its Administration on a database of their
/// own, the loop run by the test. The test reaches the database through `direct`, its own connection.
struct PanelServer {
    EventLoop loop;
    TestDatabase test;
    Database direct;
    Setup setup;
    FakeGames games;
    FakeLobby lobby;
    AddressBans bans;
    FakeDiscord fakeDiscord;
    std::optional<DatabaseThread> database;
    std::optional<PasswordHasher> hasher;
    std::optional<DiscordClient> discord;
    std::optional<PanelAccounts> accounts;
    std::optional<Administration> administration;
    Secret token = newAccessToken();
    std::optional<WebServer> web;
    std::string base;

    PanelServer(rtype::database::Engine engine, std::string_view name, Setup chosen = {})
        : test(engine, name), direct(test.open()), setup(std::move(chosen))
    {
        direct.migrate(ServerMigrations);
        const auto post = [this](std::function<void()> function) { loop.post(std::move(function)); };
        database.emplace(test.open(), post);
        hasher.emplace(post,
                       PasswordHasher::Settings{.threads = 2, .maxPending = 32, .parameters = CheapArgon2});
        discord.emplace(fakeDiscord.settings(), post);
        accounts.emplace(loop, *database, *hasher, &*discord,
                         PanelAccountsSettings{.admins = setup.admins,
                                               .publicUrl = setup.publicUrl,
                                               .discordAuthorizeUrl = "https://discord.test/oauth2/authorize",
                                               .signInsPerAddress = setup.signInsPerAddress,
                                               .addressWindow = 1min,
                                               .wrongPasswords = setup.wrongPasswords,
                                               .wrongPasswordPause = 15min,
                                               .discordLifetime = 10min,
                                               .maxDiscordStates = 256});
        administration.emplace(loop, games, lobby, bans, &*database,
                               AdministrationSettings{.version = "test",
                                                      .lobbyPort = 4242,
                                                      .gamePort = 4242,
                                                      .maxGames = 57,
                                                      .panelPort = 0,
                                                      .admins = setup.admins});
        WebServerSettings settings;
        settings.listen = Address::loopbackIpv4(0);
        settings.accessToken = token;
        settings.publicUrl = setup.publicUrl;
        settings.trustedProxies = setup.trustedProxies;
        web.emplace(loop, settings);
        base = std::format("http://{}", web->localAddress());
        web->setAdministration(&*administration);
        web->setPanelAccounts(&*accounts);
    }

    ~PanelServer()
    {
        web->stop();
        web->setPanelAccounts(nullptr);
        web->setAdministration(nullptr);
        administration.reset();
        accounts.reset();
        discord.reset();
        hasher.reset();
        database.reset();
    }

    PanelServer(const PanelServer &) = delete;
    PanelServer &operator=(const PanelServer &) = delete;

    /// A request to the web server, the loop running meanwhile: the API answers from it.
    HttpResponse call(const std::string &path, const HttpRequest &request = {}, const std::string &body = {})
    {
        auto answer =
            std::async(std::launch::async, [&] { return testing::http(base + path, request, body); });
        REQUIRE(waitUntil(loop, [&] { return answer.wait_for(0ms) == std::future_status::ready; }));
        return answer.get();
    }

    /// An account made in the database, its email verified unless told otherwise.
    Account seed(const std::string &username, const std::string &email,
                 AccountRole role = AccountRole::Player, bool verified = true)
    {
        const auto created = createAccount(direct,
                                           {.username = username,
                                            .email = email,
                                            .passwordHash = hashPassword(Password, CheapArgon2),
                                            .role = role},
                                           std::chrono::system_clock::now());
        REQUIRE(std::holds_alternative<Account>(created));
        Account account = std::get<Account>(created);
        if (verified) {
            markEmailVerified(direct, account.id, std::chrono::system_clock::now());
        }
        return account;
    }

    /// POST /api/sign-in, from the loopback, or through a proxy with `headers`.
    HttpResponse signIn(const std::string &login, const std::string &password,
                        std::vector<std::string> headers = {})
    {
        headers.emplace_back("Content-Type: application/json");
        crow::json::wvalue body;
        body["login"] = login;
        body["password"] = password;
        return call("/api/sign-in", {.method = "POST", .headers = headers}, body.dump());
    }

    /// The administrations waiting for the database have answered.
    void settle()
    {
        REQUIRE(waitUntil(loop, [&] { return database->pending() == 0; }));
    }
};

/// A session of the panel, from its Set-Cookie: GET with its cookie, POST with its cookie and CSRF token.
struct Panel {
    PanelServer &server;
    std::string session;
    std::string csrf;

    Panel(PanelServer &opened, const HttpResponse &signedIn) : server(opened)
    {
        INFO(signedIn.body);
        REQUIRE(signedIn.status == 200);
        const std::string cookie = signedIn.header("set-cookie");
        session = cookie.substr(0, cookie.find(';'));
        const HttpResponse described = get("/api/session");
        REQUIRE(described.status == 200);
        csrf = parse(described.body)["csrfToken"].s();
    }

    [[nodiscard]] HttpResponse get(const std::string &path) const
    {
        return server.call(path, {.method = "GET", .headers = {"Cookie: " + session}});
    }

    [[nodiscard]] HttpResponse post(const std::string &path, const std::string &body = "",
                                    bool withCsrf = true) const
    {
        std::vector<std::string> headers = {"Cookie: " + session, "Content-Type: application/json"};
        if (withCsrf) {
            headers.push_back("X-CSRF-Token: " + csrf);
        }
        return server.call(path, {.method = "POST", .headers = headers}, body);
    }

    [[nodiscard]] std::unique_ptr<testing::WebSocketClient> events() const
    {
        return std::make_unique<testing::WebSocketClient>(
            server.loop, server.web->localAddress(),
            std::vector<std::string>{"Cookie: " + session,
                                     std::format("Origin: http://{}", server.web->localAddress())});
    }
};

/// The value of a parameter of the query of an address.
std::string queryOf(const std::string &url, const char *name)
{
    const crow::query_string query(url.substr(url.find('?')));
    const char *value = query.get(name);
    return value != nullptr ? value : "";
}

/// The actions recorded, newest first.
std::vector<AdminAction> recorded(PanelServer &server)
{
    server.settle();
    return readActions(server.direct, 100);
}

} // namespace

// --- The rule ---

TEST_CASE("The administrators of the settings are named by a verified email or a Discord id",
          "[server][panel]")
{
    const std::vector<std::string> admins = {" Chief@Example.org ", "discord:42"};
    CHECK(isListedAdmin(admins, "chief@example.org", std::nullopt));
    CHECK(isListedAdmin(admins, "CHIEF@EXAMPLE.ORG", "7"));
    CHECK(isListedAdmin(admins, std::nullopt, "42"));
    // An email not verified names nobody, nor another id, nor a username
    CHECK_FALSE(isListedAdmin(admins, std::nullopt, std::nullopt));
    CHECK_FALSE(isListedAdmin(admins, "other@example.org", "421"));
    CHECK_FALSE(isListedAdmin({"Chief"}, "chief@example.org", std::nullopt));
    CHECK_FALSE(isListedAdmin({}, "chief@example.org", "42"));
}

// --- Who gets in ---

TEST_CASE("Who gets in: no session, the local token, a player, an admin, a banned admin",
          "[server][panel][database]")
{
    FOR_EACH_ENGINE();
    PanelServer server(engine, "panel_who");
    const Account player = server.seed("Pilot", "pilot@example.org");
    const Account admin = server.seed("Chief", "chief@example.org", AccountRole::Admin);
    const Account unverified = server.seed("Rookie", "rookie@example.org", AccountRole::Admin, false);

    // No session
    CHECK(server.call("/api/server").status == 401);
    const HttpResponse offered = server.call("/api/sign-in");
    REQUIRE(offered.status == 200);
    CHECK(parse(offered.body)["password"].b());
    CHECK_FALSE(parse(offered.body)["discord"].b());

    // The local token still opens the panel, as the actor `local`
    const HttpResponse opened = server.call("/?token=" + formatSecret(server.token));
    REQUIRE(opened.status == 303);
    const std::string local = opened.header("set-cookie").substr(0, opened.header("set-cookie").find(';'));
    const HttpResponse localSession =
        server.call("/api/session", {.method = "GET", .headers = {"Cookie: " + local}});
    REQUIRE(localSession.status == 200);
    CHECK(parse(localSession.body)["account"].t() == crow::json::type::Null);

    // A player, a wrong password, a login nobody has, an email not verified
    CHECK(server.signIn("Pilot", Password).status == 403);
    CHECK_THAT(server.signIn("pilot@example.org", Password).body, ContainsSubstring("not an administrator"));
    const HttpResponse wrong = server.signIn("Chief", "wrong password!");
    CHECK(wrong.status == 401);
    CHECK(server.signIn("Nobody", Password).body == wrong.body);
    CHECK(server.signIn("Rookie", Password).status == 403);
    CHECK_THAT(server.signIn("Rookie", Password).body, ContainsSubstring("not verified"));
    // A sign-in needs JSON: a form of another site cannot send it without asking first
    CHECK(server
              .call("/api/sign-in", {.method = "POST", .headers = {"Content-Type: text/plain"}},
                    R"({"login":"Chief","password":"correct horse battery"})")
              .status == 415);

    // An admin, by username or email
    Captured logs;
    const HttpResponse signedIn = server.signIn("CHIEF@example.org", Password);
    REQUIRE(signedIn.status == 200);
    const std::string cookie = signedIn.header("set-cookie");
    CHECK_THAT(cookie, StartsWith("rtype_session="));
    CHECK_THAT(cookie, ContainsSubstring("HttpOnly"));
    CHECK_THAT(cookie, ContainsSubstring("SameSite=Strict"));
    CHECK_THAT(cookie, !ContainsSubstring("Secure"));
    CHECK(parse(signedIn.body)["account"]["username"].s() == "Chief");
    const Panel panel(server, signedIn);
    CHECK(parse(signedIn.body)["csrfToken"].s() == panel.csrf);
    CHECK(panel.get("/api/server").status == 200);
    const std::string token = panel.session.substr(panel.session.find('=') + 1);
    CHECK_THAT(logs.sink->text(), !ContainsSubstring(Password));
    CHECK_THAT(logs.sink->text(), !ContainsSubstring(token));

    // A change needs the CSRF token of the session, and is recorded with the account
    CHECK(panel.post("/api/messages", R"({"text":"Hello"})", false).status == 403);
    CHECK(panel.post("/api/messages", R"({"text":"Hello"})").status == 200);
    const std::vector<AdminAction> actions = recorded(server);
    REQUIRE_FALSE(actions.empty());
    CHECK(actions.front().actor == std::to_string(admin.id));

    // A session of the game opens nothing in the panel
    const SessionToken game =
        createSession(server.direct, admin.id, SessionClient::Game, std::chrono::system_clock::now());
    CHECK(server
              .call("/api/server",
                    {.method = "GET", .headers = {"Cookie: rtype_session=" + formatSecret(game)}})
              .status == 401);
    CHECK(server
              .call("/api/server",
                    {.method = "GET", .headers = {"Cookie: rtype_session=" + formatSecret(newAccessToken())}})
              .status == 401);

    // Banned: its session ends at once, and it cannot sign in again
    REQUIRE(banAccount(server.direct, admin.id, "rogue", std::chrono::system_clock::now()));
    CHECK(panel.get("/api/server").status == 401);
    const HttpResponse banned = server.signIn("Chief", Password);
    CHECK(banned.status == 403);
    CHECK_THAT(banned.body, ContainsSubstring("banned"));
    (void)player;
    (void)unverified;
}

TEST_CASE("Signing out ends the session, of an account and of the local token", "[server][panel][database]")
{
    PanelServer server(rtype::database::Engine::Sqlite, "panel_sign_out");
    server.seed("Chief", "chief@example.org", AccountRole::Admin);
    const Panel panel(server, server.signIn("Chief", Password));

    CHECK(panel.post("/api/sign-out", "", false).status == 403);
    const HttpResponse out = panel.post("/api/sign-out");
    REQUIRE(out.status == 200);
    CHECK_THAT(out.header("set-cookie"), StartsWith("rtype_session=;"));
    CHECK_THAT(out.header("set-cookie"), ContainsSubstring("Max-Age=0"));
    CHECK(panel.get("/api/server").status == 401);

    const HttpResponse opened = server.call("/?token=" + formatSecret(server.token));
    const std::string local = opened.header("set-cookie").substr(0, opened.header("set-cookie").find(';'));
    const HttpResponse described =
        server.call("/api/session", {.method = "GET", .headers = {"Cookie: " + local}});
    const std::string csrf = parse(described.body)["csrfToken"].s();
    CHECK(server
              .call("/api/sign-out",
                    {.method = "POST", .headers = {"Cookie: " + local, "X-CSRF-Token: " + csrf}})
              .status == 200);
    CHECK(server.call("/api/server", {.method = "GET", .headers = {"Cookie: " + local}}).status == 401);
}

TEST_CASE("An administrator of the settings is one whatever its role, and the panel cannot demote, ban nor "
          "delete it",
          "[server][panel][database]")
{
    FOR_EACH_ENGINE();
    PanelServer server(engine, "panel_listed", Setup::with([](Setup &setup) {
                           setup.admins = {"chief@example.org", "discord:4242"};
                       }));
    const Account chief = server.seed("Chief", "Chief@Example.org");
    const Account linked = server.seed("Linked", "linked@example.org");
    REQUIRE(linkDiscord(server.direct, linked.id, "4242", std::chrono::system_clock::now()) ==
            DiscordLink::Linked);
    const Account other = server.seed("Other", "other@example.org", AccountRole::Admin);

    // At start, the role admin is given to those who have an account
    std::optional<std::optional<std::size_t>> promoted;
    server.accounts->promoteListed([&](std::optional<std::size_t> count) { promoted = count; });
    REQUIRE(waitUntil(server.loop, [&] { return promoted.has_value(); }));
    CHECK(*promoted == std::optional<std::size_t>(2));
    CHECK(findAccount(server.direct, chief.id)->role == AccountRole::Admin);
    CHECK(findAccount(server.direct, linked.id)->role == AccountRole::Admin);

    // Demoted by hand in the database, it stays an administrator
    REQUIRE(setRole(server.direct, chief.id, AccountRole::Player));
    const Panel panel(server, server.signIn("chief@example.org", Password));
    CHECK(parse(panel.get("/api/session").body)["account"]["locked"].b());

    const Panel otherPanel(server, server.signIn("Other", Password));
    const HttpResponse listed = otherPanel.get("/api/accounts");
    REQUIRE(listed.status == 200);
    const crow::json::rvalue accounts = parse(listed.body);
    for (const crow::json::rvalue &account : accounts) {
        const bool locked = account["id"].i() == chief.id || account["id"].i() == linked.id;
        CHECK(account["locked"].b() == locked);
        if (locked) {
            CHECK(account["role"].s() == "admin");
        }
    }
    for (const long long id : {chief.id, linked.id}) {
        CAPTURE(id);
        CHECK(otherPanel.post(std::format("/api/accounts/{}/role", id), R"({"role":"player"})").status ==
              409);
        CHECK(otherPanel.post(std::format("/api/accounts/{}/ban", id), R"({"reason":"coup"})").status == 409);
        CHECK(otherPanel.post(std::format("/api/accounts/{}/delete", id)).status == 409);
    }
    CHECK_FALSE(findAccount(server.direct, chief.id)->banned());
    // The others can be
    CHECK(panel.post(std::format("/api/accounts/{}/role", other.id), R"({"role":"player"})").status == 200);
    CHECK(otherPanel.get("/api/server").status == 401);
}

// --- The limits ---

TEST_CASE(
    "The limits on signing in: per address, through a trusted proxy and an untrusted one, and per account",
    "[server][panel][database]")
{
    SECTION("Through a trusted proxy, the address it forwards")
    {
        PanelServer server(rtype::database::Engine::Sqlite, "panel_limits_trusted",
                           Setup::with([](Setup &setup) {
                               setup.trustedProxies = {{*Address::parse("127.0.0.1", 0), 32}};
                               setup.signInsPerAddress = 3;
                           }));
        server.seed("Chief", "chief@example.org", AccountRole::Admin);
        for (int i = 0; i < 3; ++i) {
            CHECK(server.signIn("Chief", "wrong password!", {"X-Forwarded-For: 203.0.113.9"}).status == 401);
        }
        CHECK(server.signIn("Chief", Password, {"X-Forwarded-For: 203.0.113.9"}).status == 429);
        // Another visitor behind the same proxy
        CHECK(server.signIn("Chief", Password, {"X-Forwarded-For: 203.0.113.10"}).status == 200);
    }
    SECTION("From an untrusted address, its own, whatever it forwards")
    {
        PanelServer server(rtype::database::Engine::Sqlite, "panel_limits_untrusted",
                           Setup::with([](Setup &setup) { setup.signInsPerAddress = 3; }));
        server.seed("Chief", "chief@example.org", AccountRole::Admin);
        for (int i = 0; i < 3; ++i) {
            CHECK(server.signIn("Chief", "wrong password!", {std::format("X-Forwarded-For: 203.0.113.{}", i)})
                      .status == 401);
        }
        CHECK(server.signIn("Chief", Password, {"X-Forwarded-For: 198.51.100.1"}).status == 429);
    }
    SECTION("Per account, after its wrong passwords")
    {
        PanelServer server(rtype::database::Engine::Sqlite, "panel_limits_account",
                           Setup::with([](Setup &setup) { setup.wrongPasswords = 3; }));
        server.seed("Chief", "chief@example.org", AccountRole::Admin);
        for (int i = 0; i < 3; ++i) {
            CHECK(server.signIn("Chief", "wrong password!").status == 401);
        }
        CHECK(server.signIn("Chief", Password).status == 429);
    }
}

// --- Discord ---

TEST_CASE("The return from Discord signs in an administrator with its Lax cookie, and nothing else",
          "[server][panel][discord][database]")
{
    FOR_EACH_ENGINE();
    PanelServer server(engine, "panel_discord",
                       Setup::with([](Setup &setup) { setup.publicUrl = std::string(PublicUrl); }));
    const Account admin = server.seed("Chief", "chief@example.org", AccountRole::Admin);
    const Account player = server.seed("Pilot", "pilot@example.org");
    REQUIRE(linkDiscord(server.direct, admin.id, "1001", std::chrono::system_clock::now()) ==
            DiscordLink::Linked);
    REQUIRE(linkDiscord(server.direct, player.id, "1002", std::chrono::system_clock::now()) ==
            DiscordLink::Linked);
    const std::string callback = std::string(PublicUrl) + "/auth/discord/callback";
    CHECK(parse(server.call("/api/sign-in").body)["discord"].b());

    /// Starts a sign-in: the state, the challenge and the cookie of the browser.
    struct Started {
        std::string state;
        std::string challenge;
        std::string cookie;
    };
    const auto start = [&] {
        const HttpResponse response = server.call("/auth/discord");
        REQUIRE(response.status == 303);
        const std::string location = response.header("location");
        CHECK_THAT(location, StartsWith("https://discord.test/oauth2/authorize?"));
        CHECK(queryOf(location, "client_id") == FakeDiscord::ClientId);
        CHECK(queryOf(location, "scope") == "identify");
        CHECK(queryOf(location, "redirect_uri") == callback);
        CHECK(queryOf(location, "code_challenge_method") == "S256");
        const std::string cookie = response.header("set-cookie");
        CHECK_THAT(cookie, ContainsSubstring("Path=/auth/discord"));
        CHECK_THAT(cookie, ContainsSubstring("HttpOnly"));
        CHECK_THAT(cookie, ContainsSubstring("SameSite=Lax"));
        CHECK_THAT(cookie, ContainsSubstring("Secure"));
        return Started{queryOf(location, "state"), queryOf(location, "code_challenge"),
                       cookie.substr(0, cookie.find(';'))};
    };
    const auto back = [&](const Started &started, const std::string &code, const std::string &cookie) {
        return server.call(std::format("/auth/discord/callback?code={}&state={}", code, started.state),
                           {.method = "GET", .headers = {"Cookie: " + cookie}});
    };
    const DiscordUser chief{.id = "1001", .username = "chief"};

    SECTION("An administrator comes back signed in")
    {
        const Started started = start();
        const HttpResponse response =
            back(started, server.fakeDiscord.authorize(chief, started.challenge, callback), started.cookie);
        INFO(response.body);
        REQUIRE(response.status == 303);
        CHECK(response.header("location") == "/");
        // The session, Strict and Secure behind HTTPS; the cookie of the state removed
        CHECK_THAT(response.cookie("rtype_discord"), ContainsSubstring("Max-Age=0"));
        const std::string session = response.cookie("rtype_session");
        CHECK_THAT(session, StartsWith("rtype_session="));
        CHECK_THAT(session, ContainsSubstring("SameSite=Strict"));
        CHECK_THAT(session, ContainsSubstring("Secure"));
        const std::string cookie = session.substr(0, session.find(';'));
        CHECK(server.call("/api/server", {.method = "GET", .headers = {"Cookie: " + cookie}}).status == 200);

        // The state is single use
        CHECK(back(started, server.fakeDiscord.authorize(chief, started.challenge, callback), started.cookie)
                  .status == 403);
    }
    SECTION("A link opened in another browser signs nobody in")
    {
        const Started started = start();
        const Started mine = start();
        const std::string code = server.fakeDiscord.authorize(chief, started.challenge, callback);
        CHECK(back(started, code, "rtype_other=1").status == 403);
        CHECK(back(started, code, mine.cookie).status == 403);
        CHECK(server.fakeDiscord.exchanges() == 0);
    }
    SECTION("Discord refusing, a player, a user without account, a sign-in cancelled")
    {
        Started started = start();
        CHECK(back(started, "not-a-code", started.cookie).status == 403);
        started = start();
        const HttpResponse playerBack = back(
            started,
            server.fakeDiscord.authorize({.id = "1002", .username = "pilot"}, started.challenge, callback),
            started.cookie);
        CHECK(playerBack.status == 403);
        CHECK_THAT(playerBack.body, ContainsSubstring("not an administrator"));
        CHECK_THAT(playerBack.header("set-cookie"), ContainsSubstring("Max-Age=0"));
        started = start();
        CHECK(back(started,
                   server.fakeDiscord.authorize({.id = "1003", .username = "stranger"}, started.challenge,
                                                callback),
                   started.cookie)
                  .status == 403);
        started = start();
        CHECK(server
                  .call("/auth/discord/callback?error=access_denied&state=" + started.state,
                        {.method = "GET", .headers = {"Cookie: " + started.cookie}})
                  .status == 403);
    }
    SECTION("Behind HTTPS, the session of the local token is Secure too")
    {
        const HttpResponse opened = server.call("/?token=" + formatSecret(server.token));
        CHECK_THAT(opened.header("set-cookie"), ContainsSubstring("Secure"));
    }
}

TEST_CASE("Without web.public_url, the panel offers no Discord", "[server][panel][discord][database]")
{
    PanelServer server(rtype::database::Engine::Sqlite, "panel_no_discord");
    CHECK_FALSE(parse(server.call("/api/sign-in").body)["discord"].b());
    CHECK(server.call("/auth/discord").status == 404);
}

// --- The accounts ---

TEST_CASE("From the panel, an administrator searches, bans, unbans, promotes and deletes accounts",
          "[server][panel][database]")
{
    FOR_EACH_ENGINE();
    PanelServer server(engine, "panel_accounts");
    const Account admin = server.seed("Chief", "chief@example.org", AccountRole::Admin);
    const Account pilot = server.seed("Pilot_7", "pilot@example.org");
    const Account other = server.seed("Wingman", "wingman@example.org");
    const SessionToken game =
        createSession(server.direct, pilot.id, SessionClient::Game, std::chrono::system_clock::now());
    server.lobby.players.push_back({.id = 7,
                                    .name = "Pilot_7",
                                    .address = *Address::parse("203.0.113.9", 0),
                                    .gameId = 3,
                                    .joinedAt = std::chrono::system_clock::now(),
                                    .accountId = pilot.id});
    const Panel panel(server, server.signIn("Chief", Password));

    // Search, newest first
    const crow::json::rvalue all = parse(panel.get("/api/accounts").body);
    REQUIRE(all.size() == 3);
    CHECK(all[0]["username"].s() == "Wingman");
    const crow::json::rvalue found = parse(panel.get("/api/accounts?search=PILOT").body);
    REQUIRE(found.size() == 1);
    CHECK(found[0]["id"].i() == pilot.id);
    CHECK(found[0]["email"].s() == "pilot@example.org");
    CHECK(found[0]["role"].s() == "player");
    CHECK_FALSE(found[0]["locked"].b());
    CHECK(parse(panel.get(std::format("/api/accounts?limit=1&before={}", other.id)).body)[0]["id"].i() ==
          pilot.id);
    CHECK(panel.get("/api/accounts?limit=0").status == 400);
    CHECK(parse(panel.get(std::format("/api/accounts/{}", pilot.id)).body)["username"].s() == "Pilot_7");
    CHECK(panel.get("/api/accounts/999").status == 404);

    // Banned: its sessions end, its player is removed
    const HttpResponse banned =
        panel.post(std::format("/api/accounts/{}/ban", pilot.id), R"({"reason":"cheating"})");
    INFO(banned.body);
    REQUIRE(banned.status == 200);
    CHECK(parse(banned.body)["removed"].i() == 1);
    CHECK(parse(banned.body)["account"]["banReason"].s() == "cheating");
    CHECK(server.lobby.disconnected ==
          std::vector<std::pair<std::uint32_t, ErrorCode>>{{7, ErrorCode::Banned}});
    CHECK(server.games.kicked == std::vector<std::uint32_t>{7});
    CHECK_FALSE(resumeSession(server.direct, game, SessionClient::Game, std::chrono::system_clock::now()));
    CHECK(panel.post(std::format("/api/accounts/{}/ban", pilot.id), R"({"reason":3})").status == 400);
    CHECK(panel.post("/api/accounts/999/ban").status == 404);

    CHECK(panel.post(std::format("/api/accounts/{}/unban", pilot.id), R"({"reason":"appeal"})").status ==
          200);
    CHECK_FALSE(findAccount(server.direct, pilot.id)->banned());

    // Promoted, it signs in; demoted, its session opens nothing, and its WebSocket closes
    CHECK(panel.post(std::format("/api/accounts/{}/role", pilot.id), R"({"role":"emperor"})").status == 400);
    CHECK(panel.post(std::format("/api/accounts/{}/role", pilot.id), R"({"role":"admin"})").status == 200);
    const Panel promoted(server, server.signIn("Pilot_7", Password));
    const auto socket = promoted.events();
    REQUIRE(waitUntil(server.loop, [&] { return !socket->messages.empty(); }));
    CHECK(panel.post(std::format("/api/accounts/{}/role", pilot.id), R"({"role":"player"})").status == 200);
    CHECK(waitUntil(server.loop, [&] { return socket->closed; }));
    CHECK(promoted.get("/api/server").status == 401);

    // Deleted: gone, its actions anonymized
    CHECK(panel.post(std::format("/api/accounts/{}/delete", other.id), R"({"reason":"asked"})").status ==
          200);
    CHECK_FALSE(findAccount(server.direct, other.id));

    std::vector<std::string> kinds;
    for (const AdminAction &action : recorded(server)) {
        CHECK(action.actor == std::to_string(admin.id));
        kinds.push_back(action.action);
    }
    CHECK(kinds ==
          std::vector<std::string>{"delete_account", "role", "role", "unban_account", "ban_account"});

    // The actions of a deleted account lose its name, as those done to it
    REQUIRE(deleteAccount(server.direct, pilot.id));
    for (const AdminAction &action : readActions(server.direct, 100)) {
        CHECK_THAT(action.details, !ContainsSubstring("Pilot_7"));
    }
}

TEST_CASE("The panel shows the Discord profile of the accounts, and serves their picture",
          "[server][panel][discord][database]")
{
    FOR_EACH_ENGINE();
    PanelServer server(engine, "panel_profiles");
    const Account admin = server.seed("Chief", "chief@example.org", AccountRole::Admin);
    const Account pilot = server.seed("Pilot_7", "pilot@example.org");
    const std::string png = std::string("\x89PNG\r\n\x1a\n", 8) + "picture";
    REQUIRE(linkDiscord(server.direct, admin.id, "80351110224678912", std::chrono::system_clock::now()) ==
            DiscordLink::Linked);
    saveDiscordProfile(server.direct, "80351110224678912",
                       {.username = "chief", .displayName = "The Chief", .avatarHash = "a1b2", .avatar = png},
                       std::chrono::system_clock::now());
    const Panel panel(server, server.signIn("Chief", Password));

    // In the session, and in the list of the accounts: the names and the version of the picture
    const crow::json::rvalue session = parse(panel.get("/api/session").body);
    CHECK(session["account"]["discord"]["displayName"].s() == "The Chief");
    CHECK(session["account"]["discord"]["avatar"].s() == "a1b2");
    const crow::json::rvalue chief = parse(panel.get("/api/accounts?search=the%20chief").body);
    REQUIRE(chief.size() == 1);
    CHECK(chief[0]["discord"]["username"].s() == "chief");
    CHECK(parse(panel.get(std::format("/api/accounts/{}", pilot.id)).body)["discord"].t() ==
          crow::json::type::Null);

    // The picture itself, kept by the browser until its version changes
    const HttpResponse picture = panel.get(std::format("/api/accounts/{}/avatar", admin.id));
    REQUIRE(picture.status == 200);
    CHECK(picture.header("content-type") == "image/png");
    CHECK(picture.header("etag") == "\"a1b2\"");
    CHECK(picture.body == png);
    CHECK(panel.get(std::format("/api/accounts/{}/avatar", pilot.id)).status == 404);
    CHECK(server.call(std::format("/api/accounts/{}/avatar", admin.id)).status == 401);
}

TEST_CASE("A WebSocket opens with the session of an account, and closes once it is banned or signed out",
          "[server][panel][database]")
{
    PanelServer server(rtype::database::Engine::Sqlite, "panel_events");
    const Account first = server.seed("Chief", "chief@example.org", AccountRole::Admin);
    server.seed("Second", "second@example.org", AccountRole::Admin);
    const Panel chief(server, server.signIn("Chief", Password));
    const Panel second(server, server.signIn("Second", Password));

    const auto socket = chief.events();
    REQUIRE(waitUntil(server.loop, [&] { return !socket->messages.empty(); }));
    CHECK(socket->status == 101);
    const auto other = second.events();
    REQUIRE(waitUntil(server.loop, [&] { return !other->messages.empty(); }));

    // An unknown session is refused at the upgrade
    testing::WebSocketClient stranger(server.loop, server.web->localAddress(),
                                      {"Cookie: rtype_session=" + formatSecret(newAccessToken()),
                                       std::format("Origin: http://{}", server.web->localAddress())});
    REQUIRE(waitUntil(server.loop, [&] { return stranger.status.has_value(); }));
    CHECK(*stranger.status == 401);

    CHECK(second.post(std::format("/api/accounts/{}/ban", first.id)).status == 200);
    CHECK(waitUntil(server.loop, [&] { return socket->closed; }));
    // Refused again at the upgrade
    const auto again = chief.events();
    REQUIRE(waitUntil(server.loop, [&] { return again->status.has_value(); }));
    CHECK(*again->status == 401);

    CHECK(second.post("/api/sign-out").status == 200);
    CHECK(waitUntil(server.loop, [&] { return other->closed; }));
}
