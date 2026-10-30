// Tests of the accounts in the lobby of the master (docs/architecture/accounts.md#in-the-lobby): real TLS
// clients against a real lobby, a mailer of the tests that keeps the codes, on every engine of the database.
// Every flow of lobby protocol 3 and its errors, the answers in order, the limits, and the purge.

#include "Accounts/AccountRules.hpp"
#include "Accounts/AccountService.hpp"
#include "Accounts/AccountStore.hpp"
#include "Accounts/MailMessages.hpp"
#include "Accounts/PasswordHasher.hpp"
#include "Common/Secret.hpp"
#include "FakeMailer.hpp"
#include "GameServers.hpp"
#include "LobbyClient.hpp"
#include "Master/AdminStore.hpp"
#include "Master/Games.hpp"
#include "Master/Lobby.hpp"
#include "Master/ServerDatabase.hpp"
#include "TestDatabase.hpp"
#include "rtype/database/Database.hpp"
#include "rtype/database/DatabaseThread.hpp"
#include "rtype/lobby/Messages.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/TcpListener.hpp"
#include "rtype/network/Tls.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

using namespace rtype::server;
using namespace std::chrono_literals;
using rtype::database::Database;
using rtype::database::DatabaseThread;
using rtype::lobby::ErrorCode;
using rtype::network::Address;
using rtype::network::EventLoop;
using rtype::network::ServerIdentity;
using rtype::network::TcpListener;
using rtype::test::TestDatabase;
using testing::FakeMailer;
using testing::LobbyClient;
using testing::waitUntil;
using Bytes = std::vector<std::byte>;
using Catch::Matchers::ContainsSubstring;
namespace message_type = rtype::lobby::message_type;

namespace
{

/// Cheap hashes: these tests do not test Argon2.
constexpr Argon2Parameters CheapArgon2{.memory = 64, .passes = 1, .lanes = 1};

const std::string Password = "correct horse battery";

/// What a test changes of the master.
struct Setup {
    AccountServiceSettings accounts{.guests = true,
                                    .limits = {.requestsPerAddress = 1000,
                                               .addressWindow = 1min,
                                               .wrongPasswords = 10,
                                               .wrongPasswordPause = 15min,
                                               .resendInterval = 1min,
                                               .purgeInterval = 1h}};
    PasswordHasher::Settings hasher{.threads = 2, .maxPending = 32, .parameters = CheapArgon2};
    /// Games run by real instances, for the tests that play one.
    bool games = false;
};

/// A lobby with accounts, its database of its own, and a fake mailer. The test reaches the database through
/// `direct`, its own connection, before start() and between the requests.
struct Server {
    EventLoop loop;
    TestDatabase test;
    Database direct;
    Setup setup;
    ServerIdentity identity = ServerIdentity::generate();
    Games games{loop, setup.games ? testing::instances()
                                  : GamesSettings{.executable = RTYPE_TEST_CHILD,
                                                  .leadingArguments = {},
                                                  .trailingArguments = {}}};
    FakeMailer mailer;
    std::optional<DatabaseThread> database;
    std::optional<PasswordHasher> hasher;
    std::optional<AccountService> accounts;
    std::optional<Lobby> lobby;

    Server(rtype::database::Engine engine, std::string_view name, Setup chosen = {})
        : test(engine, name), direct(test.open()), setup(chosen)
    {
        direct.migrate(ServerMigrations);
    }

    ~Server()
    {
        lobby.reset();
        accounts.reset();
        hasher.reset();
        database.reset();
    }

    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

    void start()
    {
        const auto post = [this](std::function<void()> function) { loop.post(std::move(function)); };
        database.emplace(test.open(), post);
        hasher.emplace(post, setup.hasher);
        accounts.emplace(loop, *database, *hasher, mailer, setup.accounts);
        lobby.emplace(loop, TcpListener(loop, Address::loopbackIpv4(0)), identity, games);
        lobby->setAccounts(&*accounts);
    }

    /// Lets the database thread finish what it was given.
    void settle()
    {
        REQUIRE(waitUntil(loop, [&] { return database->pending() == 0; }));
    }

    /// An account made in the database, its email verified unless told otherwise.
    Account seed(const std::string &username, const std::string &email, bool verified = true,
                 std::chrono::system_clock::time_point at = std::chrono::system_clock::now())
    {
        const auto created = createAccount(direct,
                                           {.username = username,
                                            .email = email,
                                            .passwordHash = hashPassword(Password, CheapArgon2),
                                            .role = AccountRole::Player},
                                           at);
        REQUIRE(std::holds_alternative<Account>(created));
        Account account = std::get<Account>(created);
        if (verified) {
            markEmailVerified(direct, account.id, at);
        }
        return account;
    }
};

/// A client of the lobby, whose requests are matched with their replies in order.
struct Player {
    EventLoop &loop;
    std::unique_ptr<LobbyClient> client;
    std::size_t answered = 0;

    explicit Player(Server &server)
        : loop(server.loop), client(std::make_unique<LobbyClient>(server.loop, server.lobby->localAddress(),
                                                                  server.identity.fingerprint()))
    {
        REQUIRE(
            waitUntil(loop, [&] { return client->connection.has_value() || client->closed.has_value(); }));
    }

    /// The next reply, once received.
    std::pair<std::uint8_t, Bytes> next()
    {
        const auto reply = client->reply(loop, answered);
        REQUIRE(reply);
        ++answered;
        return *reply;
    }

    std::pair<std::uint8_t, Bytes> ask(std::uint8_t type, std::span<const std::byte> payload = {})
    {
        client->send(type, payload);
        return next();
    }

    std::pair<std::uint8_t, Bytes> hello(const std::string &name,
                                         std::optional<rtype::lobby::SessionToken> token = std::nullopt,
                                         const std::string &locale = "en")
    {
        return ask(
            message_type::Hello,
            rtype::lobby::encode(rtype::lobby::HelloMessage{
                .version = rtype::lobby::ProtocolVersion, .name = name, .locale = locale, .token = token}));
    }

    rtype::lobby::WelcomeMessage welcome(const std::string &name,
                                         std::optional<rtype::lobby::SessionToken> token = std::nullopt,
                                         const std::string &locale = "en")
    {
        const auto reply = hello(name, token, locale);
        REQUIRE(reply.first == message_type::Welcome);
        const auto welcome = rtype::lobby::decodeWelcome(reply.second);
        REQUIRE(welcome);
        return *welcome;
    }

    /// Signs in with the password of the tests.
    rtype::lobby::SignedInMessage signIn(const std::string &login, const std::string &password = Password)
    {
        const auto reply =
            ask(message_type::SignIn, rtype::lobby::encode(rtype::lobby::SignInMessage{login, password}));
        REQUIRE(reply.first == message_type::SignedIn);
        const auto signedIn = rtype::lobby::decodeSignedIn(reply.second);
        REQUIRE(signedIn);
        return *signedIn;
    }

    [[nodiscard]] bool closed()
    {
        return waitUntil(loop, [&] { return client->closed.has_value(); });
    }
};

/// The code of an ERROR answering `request`, or nothing if the reply is not one.
std::optional<ErrorCode> errorOf(const std::pair<std::uint8_t, Bytes> &reply, std::uint8_t request)
{
    if (reply.first != message_type::Error) {
        return std::nullopt;
    }
    const auto error = rtype::lobby::decodeError(reply.second);
    if (!error || error->request != request) {
        return std::nullopt;
    }
    return error->code;
}

std::optional<rtype::lobby::SignedInMessage> signedInOf(const std::pair<std::uint8_t, Bytes> &reply)
{
    if (reply.first != message_type::SignedIn) {
        return std::nullopt;
    }
    return rtype::lobby::decodeSignedIn(reply.second);
}

Bytes registering(const std::string &username, const std::string &email,
                  const std::string &password = Password)
{
    return rtype::lobby::encode(rtype::lobby::RegisterMessage{username, email, password});
}

Bytes verifying(const std::string &login, const std::string &code)
{
    return rtype::lobby::encode(rtype::lobby::VerifyEmailMessage{login, code});
}

Bytes signingIn(const std::string &login, const std::string &password = Password)
{
    return rtype::lobby::encode(rtype::lobby::SignInMessage{login, password});
}

/// A code other than this one.
std::string otherCode(const std::string &code)
{
    return code == "000000" ? "000001" : "000000";
}

/// The player of the lobby with this name, as the administration sees it.
std::optional<AdminPlayer> adminPlayer(const Lobby &lobby, const std::string &name)
{
    for (const AdminPlayer &player : lobby.adminPlayers()) {
        if (player.name == name) {
            return player;
        }
    }
    return std::nullopt;
}

/// Captures the messages logged during a test, at every level.
class Capture : public rtype::log::Sink
{
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

} // namespace

#define FOR_EACH_ENGINE()                                                                                    \
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));                                    \
    CAPTURE(rtype::test::engineName(engine))

TEST_CASE("A player registers, verifies its email, and comes back signed in from its token alone",
          "[server][accounts][database]")
{
    FOR_EACH_ENGINE();
    Server server(engine, "lobby_accounts_register");
    server.start();

    std::optional<rtype::lobby::SessionToken> token;
    {
        Player player(server);
        const rtype::lobby::WelcomeMessage welcome = player.welcome("Pilot", std::nullopt, "fr-FR");
        CHECK(welcome.features ==
              rtype::lobby::ServerFeatures{.guests = true, .passwordAccounts = true, .discord = false});
        CHECK_FALSE(welcome.account);

        CHECK(player.ask(message_type::Register, registering("Pilot_7", " Pilot@Example.org ")).first ==
              message_type::CodeSent);
        const std::optional<Mail> mail = server.mailer.last("Pilot@Example.org");
        REQUIRE(mail);
        CHECK(mail->content.subject ==
              verificationMail(MailLanguage::French, "Pilot_7", "000000", 15min).subject);
        const std::optional<std::string> code = server.mailer.code("Pilot@Example.org");
        REQUIRE(code);

        const auto verified = signedInOf(player.ask(message_type::VerifyEmail, verifying("Pilot_7", *code)));
        REQUIRE(verified);
        CHECK(verified->account.username == "Pilot_7");
        CHECK(verified->account.role == rtype::lobby::AccountRole::Player);
        token = verified->token;

        // Signed in, the player is the account, without the mark of the guests
        const std::optional<AdminPlayer> seen = adminPlayer(*server.lobby, "Pilot_7");
        REQUIRE(seen);
        CHECK(seen->accountId == static_cast<long long>(verified->account.id));
    }

    Player back(server);
    const rtype::lobby::WelcomeMessage welcome = back.welcome("", token);
    REQUIRE(welcome.account);
    CHECK(welcome.account->username == "Pilot_7");
    CHECK(findAccountByUsername(server.direct, "pilot_7")->verified());
    REQUIRE(adminPlayer(*server.lobby, "Pilot_7"));
}

TEST_CASE("A register with an email that has an account is answered like any other",
          "[server][accounts][database]")
{
    FOR_EACH_ENGINE();
    Server server(engine, "lobby_accounts_register_taken");
    server.seed("Pilot_7", "pilot@example.org");
    server.start();
    Player player(server);
    player.welcome("Someone");

    // The same answer as a new email: only the mailbox learns the account is there
    CHECK(player.ask(message_type::Register, registering("Other", "PILOT@example.org")).first ==
          message_type::CodeSent);
    CHECK(player.ask(message_type::Register, registering("Fresh", "fresh@example.org")).first ==
          message_type::CodeSent);
    const std::optional<Mail> told = server.mailer.last("PILOT@example.org");
    REQUIRE(told);
    CHECK(told->content.subject == existingAccountMail(MailLanguage::English, "Pilot_7").subject);
    CHECK_FALSE(server.mailer.code("PILOT@example.org"));
    CHECK_FALSE(findAccountByUsername(server.direct, "Other"));
    CHECK(findAccountByUsername(server.direct, "Fresh"));

    // A username taken, ignoring case, is the only thing REGISTER tells
    CHECK(errorOf(player.ask(message_type::Register, registering("PILOT_7", "new@example.org")),
                  message_type::Register) == ErrorCode::UsernameTaken);
    CHECK(server.mailer.count("new@example.org") == 0);
}

TEST_CASE("A register is checked before anything is kept or sent", "[server][accounts][database]")
{
    FOR_EACH_ENGINE();
    Server server(engine, "lobby_accounts_register_checks");
    server.start();
    Player player(server);
    player.welcome("Someone");

    const auto refused = [&](const Bytes &payload) {
        return errorOf(player.ask(message_type::Register, payload), message_type::Register);
    };
    CHECK(refused(registering("no", "pilot@example.org")) == ErrorCode::InvalidName);
    CHECK(refused(registering("Pilote!", "pilot@example.org")) == ErrorCode::InvalidName);
    CHECK(refused(registering("Pilot_7", "not an email")) == ErrorCode::InvalidEmail);
    CHECK(refused(registering("Pilot_7", "pilot@example.org", "too short")) == ErrorCode::WeakPassword);
    CHECK(refused(registering("Pilot_7_long", "pilot@example.org", "pilot_7_LONG")) ==
          ErrorCode::WeakPassword);
    CHECK(refused(registering("Pilot_7", "pilot@example.org", "PILOT@example.org")) ==
          ErrorCode::WeakPassword);
    CHECK(server.mailer.mails.empty());
    CHECK_FALSE(findAccountByUsername(server.direct, "Pilot_7"));
}

TEST_CASE("A player signs in by username or email; a wrong password and an unknown login get the same error",
          "[server][accounts][database]")
{
    FOR_EACH_ENGINE();
    Server server(engine, "lobby_accounts_sign_in");
    const Account account = server.seed("Pilot_7", "pilot@example.org");
    server.start();

    Player byName(server);
    byName.welcome("Someone");
    CHECK(errorOf(byName.ask(message_type::SignIn, signingIn("Pilot_7", "wrong password!")),
                  message_type::SignIn) == ErrorCode::InvalidCredentials);
    CHECK(errorOf(byName.ask(message_type::SignIn, signingIn("Nobody")), message_type::SignIn) ==
          ErrorCode::InvalidCredentials);
    CHECK(errorOf(byName.ask(message_type::SignIn, signingIn("nobody@example.org")), message_type::SignIn) ==
          ErrorCode::InvalidCredentials);
    const rtype::lobby::SignedInMessage signedIn = byName.signIn("pilot_7");
    CHECK(signedIn.account.id == static_cast<std::uint64_t>(account.id));
    CHECK(signedIn.account.username == "Pilot_7");

    // Signed in, the requests that sign in or create an account are refused
    CHECK(errorOf(byName.ask(message_type::SignIn, signingIn("Pilot_7")), message_type::SignIn) ==
          ErrorCode::AlreadySignedIn);
    CHECK(errorOf(byName.ask(message_type::Register, registering("Other", "other@example.org")),
                  message_type::Register) == ErrorCode::AlreadySignedIn);

    Player byEmail(server);
    byEmail.welcome("Someone");
    const rtype::lobby::SignedInMessage second = byEmail.signIn(" PILOT@example.org");
    CHECK(second.account == signedIn.account);
    CHECK(second.token != signedIn.token);
    CHECK(server.mailer.mails.empty());
}

TEST_CASE("An account not verified gets a new code, only after its right password",
          "[server][accounts][database]")
{
    FOR_EACH_ENGINE();
    Server server(engine, "lobby_accounts_sign_in_unverified");
    server.seed("Pilot_7", "pilot@example.org", false);
    server.start();
    Player player(server);
    player.welcome("Someone");

    CHECK(errorOf(player.ask(message_type::SignIn, signingIn("Pilot_7", "wrong password!")),
                  message_type::SignIn) == ErrorCode::InvalidCredentials);
    CHECK(server.mailer.mails.empty());

    CHECK(errorOf(player.ask(message_type::SignIn, signingIn("Pilot_7")), message_type::SignIn) ==
          ErrorCode::EmailNotVerified);
    const std::optional<std::string> code = server.mailer.code("pilot@example.org");
    REQUIRE(code);
    const auto verified =
        signedInOf(player.ask(message_type::VerifyEmail, verifying("pilot@example.org", *code)));
    REQUIRE(verified);
    CHECK(verified->account.username == "Pilot_7");
}

TEST_CASE("A code is refused when wrong, unknown, expired or tried too often", "[server][accounts][database]")
{
    FOR_EACH_ENGINE();
    Server server(engine, "lobby_accounts_codes");
    const Account account = server.seed("Pilot_7", "pilot@example.org", false);
    server.start();
    Player player(server);
    player.welcome("Someone");

    CHECK(errorOf(player.ask(message_type::VerifyEmail, verifying("Nobody", "123456")),
                  message_type::VerifyEmail) == ErrorCode::InvalidCode);
    // No code made yet
    CHECK(errorOf(player.ask(message_type::VerifyEmail, verifying("Pilot_7", "123456")),
                  message_type::VerifyEmail) == ErrorCode::InvalidCode);

    CHECK(
        player.ask(message_type::ResendCode, rtype::lobby::encode(rtype::lobby::ResendCodeMessage{"Pilot_7"}))
            .first == message_type::CodeSent);
    const std::optional<std::string> code = server.mailer.code("pilot@example.org");
    REQUIRE(code);
    for (int attempt = 0; attempt < MaxCodeAttempts; ++attempt) {
        CHECK(errorOf(player.ask(message_type::VerifyEmail, verifying("Pilot_7", otherCode(*code))),
                      message_type::VerifyEmail) == ErrorCode::InvalidCode);
    }
    CHECK(errorOf(player.ask(message_type::VerifyEmail, verifying("Pilot_7", *code)),
                  message_type::VerifyEmail) == ErrorCode::TooManyAttempts);

    // A code made 16 minutes ago
    const std::string old =
        createCode(server.direct, account.id, CodePurpose::Verify, std::chrono::system_clock::now() - 16min);
    CHECK(errorOf(player.ask(message_type::VerifyEmail, verifying("Pilot_7", old)),
                  message_type::VerifyEmail) == ErrorCode::CodeExpired);
}

TEST_CASE("A code is sent again at most once a minute, whether the login has an account or not",
          "[server][accounts][database]")
{
    FOR_EACH_ENGINE();
    Server server(engine, "lobby_accounts_resend");
    server.seed("Pilot_7", "pilot@example.org", false);
    server.seed("Verified", "verified@example.org");
    server.start();
    Player player(server);
    player.welcome("Someone");

    const auto resend = [&](const std::string &login) {
        return player.ask(message_type::ResendCode,
                          rtype::lobby::encode(rtype::lobby::ResendCodeMessage{login}));
    };
    CHECK(resend("Pilot_7").first == message_type::CodeSent);
    CHECK(server.mailer.count("pilot@example.org") == 1);
    CHECK(errorOf(resend("pilot_7"), message_type::ResendCode) == ErrorCode::TooManyAttempts);
    CHECK(server.mailer.count("pilot@example.org") == 1);

    CHECK(resend("Nobody").first == message_type::CodeSent);
    CHECK(errorOf(resend("Nobody"), message_type::ResendCode) == ErrorCode::TooManyAttempts);
    // Verified already: nothing to send, the same answer
    CHECK(resend("Verified").first == message_type::CodeSent);
    CHECK(server.mailer.count("verified@example.org") == 0);
}

TEST_CASE("A forgotten password is set again with its code, and ends every session",
          "[server][accounts][database]")
{
    FOR_EACH_ENGINE();
    Server server(engine, "lobby_accounts_reset");
    const Account account = server.seed("Pilot_7", "pilot@example.org", false);
    const SessionToken before =
        createSession(server.direct, account.id, SessionClient::Game, std::chrono::system_clock::now());
    server.start();
    Player player(server);
    player.welcome("Someone");

    const auto forgot = [&](const std::string &email) {
        return player.ask(message_type::ForgotPassword,
                          rtype::lobby::encode(rtype::lobby::ForgotPasswordMessage{email}));
    };
    const auto reset = [&](const std::string &code, const std::string &password) {
        return player.ask(
            message_type::ResetPassword,
            rtype::lobby::encode(rtype::lobby::ResetPasswordMessage{"pilot@example.org", code, password}));
    };
    // The same answer whether the email has an account or not
    CHECK(forgot("nobody@example.org").first == message_type::CodeSent);
    CHECK(server.mailer.mails.empty());
    CHECK(errorOf(forgot("not an email"), message_type::ForgotPassword) == ErrorCode::InvalidEmail);
    CHECK(forgot("Pilot@Example.org").first == message_type::CodeSent);
    const std::optional<Mail> mail = server.mailer.last("pilot@example.org");
    REQUIRE(mail);
    CHECK(mail->content.subject == resetMail(MailLanguage::English, "Pilot_7", "000000", 15min).subject);
    const std::string code = *server.mailer.code("pilot@example.org");

    CHECK(errorOf(reset(otherCode(code), "a new password"), message_type::ResetPassword) ==
          ErrorCode::InvalidCode);
    CHECK(errorOf(reset(code, "short"), message_type::ResetPassword) == ErrorCode::WeakPassword);
    CHECK(errorOf(reset(code, "PILOT_7"), message_type::ResetPassword) == ErrorCode::WeakPassword);
    const auto signedIn = signedInOf(reset(code, "a new password"));
    REQUIRE(signedIn);
    CHECK(signedIn->account.username == "Pilot_7");
    // The code proved the email
    CHECK(findAccount(server.direct, account.id)->verified());

    Player old(server);
    CHECK(errorOf(old.hello("", before), message_type::Hello) == ErrorCode::SessionExpired);
    Player again(server);
    again.welcome("Someone");
    CHECK(errorOf(again.ask(message_type::SignIn, signingIn("Pilot_7")), message_type::SignIn) ==
          ErrorCode::InvalidCredentials);
    again.signIn("Pilot_7", "a new password");
}

TEST_CASE("Signing out ends the session of the connection, then closes it", "[server][accounts][database]")
{
    FOR_EACH_ENGINE();
    Server server(engine, "lobby_accounts_sign_out");
    server.seed("Pilot_7", "pilot@example.org");
    server.start();

    Player guest(server);
    guest.welcome("Someone");
    CHECK(errorOf(guest.ask(message_type::SignOut), message_type::SignOut) == ErrorCode::NotSignedIn);

    const rtype::lobby::SignedInMessage signedIn = guest.signIn("Pilot_7");
    CHECK(guest.ask(message_type::SignOut).first == message_type::Done);
    CHECK(guest.closed());

    Player back(server);
    CHECK(errorOf(back.hello("", signedIn.token), message_type::Hello) == ErrorCode::SessionExpired);
    CHECK(back.closed());
}

TEST_CASE("A token unknown or expired gets SESSION_EXPIRED, then the connection closes",
          "[server][accounts][database]")
{
    FOR_EACH_ENGINE();
    Server server(engine, "lobby_accounts_tokens");
    const Account account = server.seed("Pilot_7", "pilot@example.org");
    const SessionToken expired = createSession(server.direct, account.id, SessionClient::Game,
                                               std::chrono::system_clock::now() - GameSessionLifetime - 1h);
    const SessionToken panel =
        createSession(server.direct, account.id, SessionClient::Panel, std::chrono::system_clock::now());
    server.start();

    rtype::lobby::SessionToken unknown{};
    unknown.fill(std::byte{0x5A});
    for (const rtype::lobby::SessionToken &token : {unknown, expired, panel}) {
        Player player(server);
        CHECK(errorOf(player.hello("", token), message_type::Hello) == ErrorCode::SessionExpired);
        CHECK(player.closed());
    }
}

TEST_CASE("A banned account cannot sign in, and its sessions are ended", "[server][accounts][database]")
{
    FOR_EACH_ENGINE();
    Server server(engine, "lobby_accounts_banned");
    const Account account = server.seed("Pilot_7", "pilot@example.org");
    const SessionToken token =
        createSession(server.direct, account.id, SessionClient::Game, std::chrono::system_clock::now());
    banAccount(server.direct, account.id, "cheating", std::chrono::system_clock::now());
    server.start();

    Player resumed(server);
    CHECK(errorOf(resumed.hello("", token), message_type::Hello) == ErrorCode::SessionExpired);

    Player player(server);
    player.welcome("Someone");
    // A wrong password tells nothing of the ban
    CHECK(errorOf(player.ask(message_type::SignIn, signingIn("Pilot_7", "wrong password!")),
                  message_type::SignIn) == ErrorCode::InvalidCredentials);
    CHECK(errorOf(player.ask(message_type::SignIn, signingIn("Pilot_7")), message_type::SignIn) ==
          ErrorCode::Banned);
    CHECK(player.closed());
}

TEST_CASE("A guest cannot take the username of an account, and carries the mark of the guests",
          "[server][accounts][database]")
{
    FOR_EACH_ENGINE();
    Server server(engine, "lobby_accounts_guests");
    server.seed("Pilot_7", "pilot@example.org");
    server.start();

    Player player(server);
    CHECK(errorOf(player.hello("pilot_7"), message_type::Hello) == ErrorCode::NameReserved);
    // The connection stays, and says hello again
    player.welcome("Pilot");
    CHECK(adminPlayer(*server.lobby, "~Pilot"));
    CHECK_FALSE(adminPlayer(*server.lobby, "~Pilot")->accountId);

    Player longName(server);
    longName.welcome("Sixteen_letters_");
    CHECK(adminPlayer(*server.lobby, "~Sixteen_letters"));
    // Cut on a whole character: é is two bytes
    CHECK(markGuest("aaaaaaaaaaaaaaé") == "~aaaaaaaaaaaaaa");
    CHECK(markGuest("Pavel") == "~Pavel");
}

TEST_CASE("A master without guests welcomes them, but lets only accounts play",
          "[server][accounts][database]")
{
    FOR_EACH_ENGINE();
    Setup setup;
    setup.accounts.guests = false;
    Server server(engine, "lobby_accounts_no_guests", setup);
    server.seed("Pilot_7", "pilot@example.org");
    server.start();

    Player player(server);
    const rtype::lobby::WelcomeMessage welcome = player.welcome("Someone");
    CHECK(welcome.features ==
          rtype::lobby::ServerFeatures{.guests = false, .passwordAccounts = true, .discord = false});
    CHECK(errorOf(player.ask(message_type::CreateGame,
                             rtype::lobby::encode(rtype::lobby::CreateGameMessage{"Game", 2})),
                  message_type::CreateGame) == ErrorCode::GuestsDisabled);
    CHECK(errorOf(player.ask(message_type::JoinGame, rtype::lobby::encode(rtype::lobby::JoinGameMessage{1})),
                  message_type::JoinGame) == ErrorCode::GuestsDisabled);
    player.signIn("Pilot_7");
    CHECK(errorOf(player.ask(message_type::JoinGame, rtype::lobby::encode(rtype::lobby::JoinGameMessage{1})),
                  message_type::JoinGame) == ErrorCode::GameNotFound);
}

TEST_CASE("Requests sent back to back are answered in order", "[server][accounts][database]")
{
    FOR_EACH_ENGINE();
    Server server(engine, "lobby_accounts_order");
    server.seed("Pilot_7", "pilot@example.org", false);
    server.start();
    Player player(server);

    // HELLO waits for the database too: what follows it waits for its answer
    player.client->hello("Someone");
    player.client->send(message_type::Register, registering("Fresh", "fresh@example.org"));
    player.client->send(message_type::SignIn, signingIn("Pilot_7"));
    player.client->send(message_type::Ping);
    player.client->send(message_type::SignIn, signingIn("Pilot_7", "wrong password!"));
    player.client->send(message_type::ListGames);

    CHECK(player.next().first == message_type::Welcome);
    CHECK(player.next().first == message_type::CodeSent);
    CHECK(errorOf(player.next(), message_type::SignIn) == ErrorCode::EmailNotVerified);
    CHECK(player.next().first == message_type::Pong);
    CHECK(errorOf(player.next(), message_type::SignIn) == ErrorCode::InvalidCredentials);
    CHECK(player.next().first == message_type::GameList);
}

TEST_CASE("The limits: requests per address, wrong passwords, codes of a day, a full hasher, a full mailer",
          "[server][accounts][database]")
{
    FOR_EACH_ENGINE();

    SECTION("Requests per address")
    {
        Setup setup;
        setup.accounts.limits.requestsPerAddress = 3;
        Server server(engine, "lobby_accounts_limit_address", setup);
        server.start();
        Player player(server);
        player.welcome("Someone");
        for (int request = 0; request < 3; ++request) {
            CHECK(errorOf(player.ask(message_type::SignIn, signingIn("Nobody")), message_type::SignIn) ==
                  ErrorCode::InvalidCredentials);
        }
        CHECK(errorOf(player.ask(message_type::SignIn, signingIn("Nobody")), message_type::SignIn) ==
              ErrorCode::TooManyAttempts);
        // Every connection of the address counts
        Player other(server);
        other.welcome("Other");
        CHECK(errorOf(other.ask(message_type::ResendCode,
                                rtype::lobby::encode(rtype::lobby::ResendCodeMessage{"Nobody"})),
                      message_type::ResendCode) == ErrorCode::TooManyAttempts);
    }

    SECTION("Wrong passwords of an account")
    {
        Setup setup;
        setup.accounts.limits.wrongPasswords = 3;
        Server server(engine, "lobby_accounts_limit_passwords", setup);
        server.seed("Pilot_7", "pilot@example.org");
        server.start();
        Player player(server);
        player.welcome("Someone");
        for (int attempt = 0; attempt < 3; ++attempt) {
            CHECK(errorOf(player.ask(message_type::SignIn, signingIn("Pilot_7", "wrong password!")),
                          message_type::SignIn) == ErrorCode::InvalidCredentials);
        }
        // Paused: even the right password
        CHECK(errorOf(player.ask(message_type::SignIn, signingIn("Pilot_7")), message_type::SignIn) ==
              ErrorCode::TooManyAttempts);
    }

    SECTION("The daily limit of the codes tells the holder of the account")
    {
        Setup setup;
        setup.accounts.limits.resendInterval = 0ms;
        Server server(engine, "lobby_accounts_limit_daily", setup);
        server.seed("Pilot_7", "pilot@example.org", false);
        server.start();
        Player player(server);
        player.welcome("Someone");
        std::optional<ErrorCode> last;
        for (int round = 0; round < MaxDailyCodeFailures / MaxCodeAttempts; ++round) {
            CHECK(player
                      .ask(message_type::ResendCode,
                           rtype::lobby::encode(rtype::lobby::ResendCodeMessage{"Pilot_7"}))
                      .first == message_type::CodeSent);
            const std::string code = *server.mailer.code("pilot@example.org");
            for (int attempt = 0; attempt < MaxCodeAttempts; ++attempt) {
                last = errorOf(player.ask(message_type::VerifyEmail, verifying("Pilot_7", otherCode(code))),
                               message_type::VerifyEmail);
            }
        }
        CHECK(last == ErrorCode::TooManyAttempts);
        const std::optional<Mail> told = server.mailer.last("pilot@example.org");
        REQUIRE(told);
        CHECK(told->content.subject == tooManyCodesMail(MailLanguage::English, "Pilot_7").subject);
    }

    SECTION("A full hasher answers BUSY at once")
    {
        Setup setup;
        setup.hasher.maxPending = 0;
        Server server(engine, "lobby_accounts_limit_hasher", setup);
        server.seed("Pilot_7", "pilot@example.org");
        server.start();
        Player player(server);
        player.welcome("Someone");
        CHECK(errorOf(player.ask(message_type::SignIn, signingIn("Pilot_7")), message_type::SignIn) ==
              ErrorCode::Busy);
        CHECK(errorOf(player.ask(message_type::Register, registering("Fresh", "fresh@example.org")),
                      message_type::Register) == ErrorCode::Busy);
    }

    SECTION("A full mailer answers MAIL_UNAVAILABLE")
    {
        Server server(engine, "lobby_accounts_limit_mailer");
        server.mailer.answer = MailQueued::Busy;
        server.start();
        Player player(server);
        player.welcome("Someone");
        CHECK(errorOf(player.ask(message_type::Register, registering("Fresh", "fresh@example.org")),
                      message_type::Register) == ErrorCode::MailUnavailable);
    }
}

TEST_CASE("The accounts not verified within 24 hours are purged at start", "[server][accounts][database]")
{
    FOR_EACH_ENGINE();
    Server server(engine, "lobby_accounts_purge");
    const auto now = std::chrono::system_clock::now();
    server.seed("Old", "old@example.org", false, now - 25h);
    server.seed("Recent", "recent@example.org", false, now - 1h);
    server.seed("Verified", "verified@example.org", true, now - 25h);
    server.start();
    server.settle();

    CHECK_FALSE(findAccountByUsername(server.direct, "Old"));
    CHECK(findAccountByUsername(server.direct, "Recent"));
    CHECK(findAccountByUsername(server.direct, "Verified"));
}

TEST_CASE("A player deletes its account with its password; its actions stay, anonymized",
          "[server][accounts][database]")
{
    FOR_EACH_ENGINE();
    Server server(engine, "lobby_accounts_delete");
    const Account account = server.seed("Pilot_7", "pilot@example.org");
    const Account other = server.seed("Other", "other@example.org");
    const auto now = std::chrono::system_clock::now();
    const std::string id = std::to_string(account.id);
    recordAction(server.direct, {.id = 0,
                                 .at = now,
                                 .actor = id,
                                 .action = "message",
                                 .target = "everyone",
                                 .details = R"({"text":"hello","recipients":2})"});
    recordAction(server.direct, {.id = 0,
                                 .at = now,
                                 .actor = "local",
                                 .action = "kick",
                                 .target = "player:4",
                                 .details = R"({"name":"Pilot_7","address":"203.0.113.9","account":)" + id +
                                            R"(,"reason":""})"});
    recordAction(server.direct, {.id = 0,
                                 .at = now,
                                 .actor = "local",
                                 .action = "end_game",
                                 .target = "game:2",
                                 .details = R"({"name":"Game","players":["Other","Pilot_7"],"accounts":[)" +
                                            std::to_string(other.id) + "," + id + R"(],"reason":""})"});
    recordAction(server.direct, {.id = 0,
                                 .at = now,
                                 .actor = "local",
                                 .action = "kick",
                                 .target = "player:5",
                                 .details = R"({"name":"Other","address":"203.0.113.10","account":)" +
                                            std::to_string(other.id) + R"(,"reason":""})"});
    server.start();

    Player guest(server);
    guest.welcome("Someone");
    const auto deleting = [](const std::string &password) {
        return rtype::lobby::encode(rtype::lobby::DeleteAccountMessage{password});
    };
    CHECK(errorOf(guest.ask(message_type::DeleteAccount, deleting(Password)), message_type::DeleteAccount) ==
          ErrorCode::NotSignedIn);
    guest.signIn("Pilot_7");
    CHECK(errorOf(guest.ask(message_type::DeleteAccount, deleting("wrong password!")),
                  message_type::DeleteAccount) == ErrorCode::InvalidCredentials);
    CHECK(errorOf(guest.ask(message_type::DeleteAccount, deleting("")), message_type::DeleteAccount) ==
          ErrorCode::InvalidCredentials);
    CHECK(guest.ask(message_type::DeleteAccount, deleting(Password)).first == message_type::Done);
    CHECK(guest.closed());

    CHECK_FALSE(findAccount(server.direct, account.id));
    long long sessions = -1;
    server.direct.session() << "SELECT COUNT(*) FROM account_sessions WHERE account_id = :id",
        soci::use(account.id), soci::into(sessions);
    CHECK(sessions == 0);

    const std::vector<AdminAction> actions = readActions(server.direct, 10);
    REQUIRE(actions.size() == 4);
    // Newest first
    CHECK(actions[3].actor == "deleted");
    CHECK(actions[3].details == R"({"text":"hello","recipients":2})");
    CHECK_THAT(actions[2].details, !ContainsSubstring("Pilot_7") && !ContainsSubstring("203.0.113.9") &&
                                       !ContainsSubstring("account"));
    CHECK_THAT(actions[2].details, ContainsSubstring(R"("name":"deleted")"));
    CHECK_THAT(actions[1].details, !ContainsSubstring("Pilot_7") &&
                                       ContainsSubstring(R"("Other","deleted")") &&
                                       ContainsSubstring("[" + std::to_string(other.id) + "]"));
    // The others are kept as they were
    CHECK_THAT(actions[0].details, ContainsSubstring("Other") && ContainsSubstring("203.0.113.10"));
}

TEST_CASE("Discord is not offered yet", "[server][accounts][database]")
{
    FOR_EACH_ENGINE();
    Server server(engine, "lobby_accounts_discord");
    server.start();
    Player player(server);
    player.welcome("Someone");
    CHECK(errorOf(player.ask(message_type::DiscordSignIn), message_type::DiscordSignIn) ==
          ErrorCode::AccountsDisabled);
    CHECK(errorOf(player.ask(message_type::DiscordConfirm,
                             rtype::lobby::encode(rtype::lobby::DiscordConfirmMessage{"K7P2ZX"})),
                  message_type::DiscordConfirm) == ErrorCode::AccountsDisabled);
}

TEST_CASE("No password, code nor token of the accounts reaches the logs", "[server][accounts][database]")
{
    FOR_EACH_ENGINE();
    Server server(engine, "lobby_accounts_logs");
    auto capture = std::make_shared<Capture>();
    const rtype::log::Level previous = rtype::log::level();
    rtype::log::setSink(capture);
    rtype::log::setLevel(rtype::log::Level::Trace);

    server.start();
    Player player(server);
    player.welcome("Someone");
    CHECK(player.ask(message_type::Register, registering("Pilot_7", "pilot@example.org")).first ==
          message_type::CodeSent);
    const std::string code = *server.mailer.code("pilot@example.org");
    CHECK(errorOf(player.ask(message_type::VerifyEmail, verifying("Pilot_7", otherCode(code))),
                  message_type::VerifyEmail) == ErrorCode::InvalidCode);
    const auto verified = signedInOf(player.ask(message_type::VerifyEmail, verifying("Pilot_7", code)));
    REQUIRE(verified);
    CHECK(player.ask(message_type::SignOut).first == message_type::Done);
    Player again(server);
    again.welcome("Someone");
    again.signIn("Pilot_7");
    server.settle();

    rtype::log::setLevel(previous);
    rtype::log::setSink(nullptr);
    const std::string logs = capture->text();
    CHECK_THAT(logs, ContainsSubstring("Pilot_7"));
    CHECK_THAT(logs, !ContainsSubstring(Password));
    CHECK_THAT(logs, !ContainsSubstring(code));
    CHECK_THAT(logs, !ContainsSubstring(formatSecret(verified->token)));
    CHECK_THAT(logs, !ContainsSubstring("pilot@example.org"));
}

TEST_CASE("A player in a game cannot sign in, sign out, nor delete its account",
          "[server][accounts][database]")
{
    FOR_EACH_ENGINE();
    Setup setup;
    setup.games = true;
    Server server(engine, "lobby_accounts_in_game", setup);
    server.seed("Pilot_7", "pilot@example.org");
    server.start();
    Player player(server);
    player.welcome("Someone");

    REQUIRE(
        player.ask(message_type::CreateGame, rtype::lobby::encode(rtype::lobby::CreateGameMessage{"Game", 1}))
            .first == message_type::GameJoined);
    CHECK(errorOf(player.ask(message_type::SignIn, signingIn("Pilot_7")), message_type::SignIn) ==
          ErrorCode::AlreadyInGame);
    CHECK(errorOf(player.ask(message_type::SignOut), message_type::SignOut) == ErrorCode::AlreadyInGame);
    CHECK(player.ask(message_type::LeaveGame).first == message_type::LeftGame);
    player.signIn("Pilot_7");
    REQUIRE(
        player.ask(message_type::CreateGame, rtype::lobby::encode(rtype::lobby::CreateGameMessage{"Game", 1}))
            .first == message_type::GameJoined);
    CHECK(errorOf(player.ask(message_type::DeleteAccount,
                             rtype::lobby::encode(rtype::lobby::DeleteAccountMessage{Password})),
                  message_type::DeleteAccount) == ErrorCode::AlreadyInGame);
    CHECK(findAccountByUsername(server.direct, "Pilot_7"));
}
