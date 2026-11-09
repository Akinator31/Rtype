// Tests of the accounts of the master, without any network (docs/architecture/accounts.md): the rules of a
// username, an email and a password; the hasher of the passwords; what the database keeps of the accounts,
// their sessions and their codes, on every engine.

#include "Accounts/AccountRules.hpp"
#include "Accounts/AccountStore.hpp"
#include "Accounts/PasswordHasher.hpp"
#include "Accounts/ProgressStore.hpp"
#include "Master/AdminStore.hpp"
#include "Master/ServerDatabase.hpp"
#include "TestDatabase.hpp"
#include "rtype/database/Database.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

using namespace rtype::server;
using namespace std::chrono_literals;
using rtype::database::Database;
using rtype::test::TestDatabase;
using TimePoint = std::chrono::system_clock::time_point;

namespace
{

/// Noon (UTC) of a day: the tests of the daily limit stay within that day.
const TimePoint Noon = fromUnixMilliseconds((20'000LL * 24 + 12) * 60 * 60 * 1000);

/// A hash cheap to make: the tests of the database do not test Argon2.
const std::string SomeHash = "$argon2id$v=19$m=8,t=1,p=1$c29tZXNhbHQ$aGFzaA";

/// A database of its own for the test, with the schema of the master.
struct Fixture {
    TestDatabase test;
    Database database;

    Fixture(rtype::database::Engine engine, std::string_view name) : test(engine, name), database(test.open())
    {
        database.migrate(ServerMigrations);
    }
};

/// Makes an account that must be made.
Account create(Database &database, const std::string &username, std::optional<std::string> email,
               TimePoint now = Noon)
{
    auto created = createAccount(database,
                                 {.username = username,
                                  .email = std::move(email),
                                  .passwordHash = SomeHash,
                                  .role = AccountRole::Player},
                                 now);
    REQUIRE(std::holds_alternative<Account>(created));
    return std::get<Account>(created);
}

/// A code other than this one.
std::string otherCode(const std::string &code)
{
    return code == "000000" ? "000001" : "000000";
}

/// Stands for the event loop of the master: the results of the hasher wait here until the test runs them.
struct Posted {
    std::mutex mutex;
    std::vector<std::function<void()>> functions;

    void post(std::function<void()> function)
    {
        const std::lock_guard lock(mutex);
        functions.push_back(std::move(function));
    }
    [[nodiscard]] std::size_t size()
    {
        const std::lock_guard lock(mutex);
        return functions.size();
    }
    /// Waits for `count` results, then runs them.
    void runWhen(std::size_t count)
    {
        const auto deadline = std::chrono::steady_clock::now() + 30s;
        while (size() < count && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(1ms);
        }
        std::vector<std::function<void()>> ready;
        {
            const std::lock_guard lock(mutex);
            ready = std::exchange(functions, {});
        }
        REQUIRE(ready.size() == count);
        for (const auto &function : ready) {
            function();
        }
    }
};

/// The cheapest parameters Argon2 takes: the tests of the hasher test its threads and its queue.
constexpr Argon2Parameters CheapParameters{.memory = 8, .passes = 1, .lanes = 1};

} // namespace

// --- Rules ---

TEST_CASE("A username is 3 to 16 letters, digits, underscores or hyphens", "[server][accounts]")
{
    CHECK(isValidUsername("Bob"));
    CHECK(isValidUsername("R-Type_Pilot_007"));
    CHECK_FALSE(isValidUsername("Al"));
    CHECK_FALSE(isValidUsername("ThisNameIsTooLong"));
    CHECK_FALSE(isValidUsername("with space"));
    CHECK_FALSE(isValidUsername("élise"));
    CHECK_FALSE(isValidUsername("bob@home"));
    CHECK(usernameKey("R-Type_Pilot") == "r-type_pilot");
}

TEST_CASE("An email is trimmed, checked loosely, and compared ignoring case", "[server][accounts]")
{
    CHECK(normalizeEmail("  Pilot@Example.org \t") == "Pilot@Example.org");
    CHECK(normalizeEmail("a@b.co") == "a@b.co");
    CHECK_FALSE(normalizeEmail("pilot"));
    CHECK_FALSE(normalizeEmail("@example.org"));
    CHECK_FALSE(normalizeEmail("pilot@"));
    CHECK_FALSE(normalizeEmail("pilot@localhost"));
    CHECK_FALSE(normalizeEmail("pilot@example."));
    CHECK_FALSE(normalizeEmail("pilot@.org"));
    CHECK_FALSE(normalizeEmail("pi lot@example.org"));
    CHECK_FALSE(normalizeEmail("pilot@example.org\n\x01"));
    CHECK_FALSE(normalizeEmail(std::string(250, 'a') + "@b.co"));
    CHECK(emailKey("Pilot@Example.org") == "pilot@example.org");
}

TEST_CASE("A password is 10 to 128 characters, neither the username nor the email", "[server][accounts]")
{
    CHECK(checkPassword("correct horse", "Bob", "bob@example.org") == PasswordProblem::None);
    CHECK(checkPassword("too short", "Bob", "") == PasswordProblem::TooShort);
    CHECK(checkPassword(std::string(128, 'x'), "Bob", "") == PasswordProblem::None);
    CHECK(checkPassword(std::string(129, 'x'), "Bob", "") == PasswordProblem::TooLong);
    // Characters, not bytes: 10 accented letters are 20 bytes
    CHECK(checkPassword("ééééééééé", "Bob", "") == PasswordProblem::TooShort);
    CHECK(checkPassword("éééééééééé", "Bob", "") == PasswordProblem::None);
    CHECK(checkPassword("pass\tword\x01!", "Bob", "") == PasswordProblem::InvalidText);
    CHECK(checkPassword("\xFF\xFE\xFD\xFC\xFB\xFA\xF9\xF8\xF7\xF6", "Bob", "") ==
          PasswordProblem::InvalidText);
    CHECK(checkPassword("Pilot_Bob_007", "pilot_bob_007", "") == PasswordProblem::SameAsUsername);
    CHECK(checkPassword("BOB@EXAMPLE.ORG", "Bob", "bob@example.org") == PasswordProblem::SameAsEmail);
}

// --- Passwords ---

TEST_CASE("Argon2id gives the hash of its reference test vector", "[server][accounts]")
{
    // src/test.c of the reference implementation: "password", salt "somesalt", t=2, m=256 KiB, p=1
    const std::string vector =
        "$argon2id$v=19$m=256,t=2,p=1$c29tZXNhbHQ$nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4";
    CHECK(verifyPassword(vector, "password"));
    CHECK_FALSE(verifyPassword(vector, "Password"));
    // Argon2i or Argon2d hashes, and anything else, are refused
    CHECK_FALSE(verifyPassword(
        "$argon2i$v=19$m=65536,t=2,p=1$c29tZXNhbHQ$wWKIMhR9lyDFvRz9YTZweHKfbftvj+qf+YFY4NeBbtA", "password"));
    CHECK_FALSE(verifyPassword("not a hash", "password"));
    CHECK_FALSE(verifyPassword("", ""));
}

TEST_CASE("A password hashed with the default parameters is checked, a wrong one refused",
          "[server][accounts]")
{
    const std::string hash = hashPassword("correct horse battery");
    CHECK(hash.starts_with("$argon2id$v=19$m=19456,t=2,p=1$"));
    CHECK(verifyPassword(hash, "correct horse battery"));
    CHECK_FALSE(verifyPassword(hash, "correct horse batterY"));
    // A new salt each time
    CHECK(hashPassword("correct horse battery") != hash);
}

TEST_CASE("The hasher works on its threads and hands its results to the loop", "[server][accounts]")
{
    Posted posted;
    PasswordHasher hasher([&](std::function<void()> function) { posted.post(std::move(function)); },
                          {.threads = 2, .maxPending = 4, .parameters = CheapParameters});
    std::optional<std::string> hash;
    REQUIRE(
        hasher.hash("correct horse", [&](std::optional<std::string> result) { hash = std::move(result); }));
    posted.runWhen(1);
    REQUIRE(hash);
    CHECK(hash->starts_with("$argon2id$v=19$m=8,t=1,p=1$"));

    std::vector<bool> matches;
    REQUIRE(hasher.verify(*hash, "correct horse", [&](bool match) { matches.push_back(match); }));
    REQUIRE(hasher.verify(*hash, "wrong horse", [&](bool match) { matches.push_back(match); }));
    posted.runWhen(2);
    // In either order: two threads
    CHECK(matches.size() == 2);
    CHECK(std::ranges::count(matches, true) == 1);
    CHECK(hasher.pending() == 0);
}

TEST_CASE("The hasher refuses a request at once when its queue is full", "[server][accounts]")
{
    Posted posted;
    PasswordHasher hasher([&](std::function<void()> function) { posted.post(std::move(function)); },
                          {.threads = 1, .maxPending = 2, .parameters = CheapParameters});
    int done = 0;
    CHECK(hasher.hash("correct horse", [&](auto) { ++done; }));
    CHECK(hasher.verify(SomeHash, "correct horse", [&](bool) { ++done; }));
    // Full until the results are delivered, even once computed
    CHECK_FALSE(hasher.hash("correct horse", [&](auto) { ++done; }));
    CHECK_FALSE(hasher.verify(SomeHash, "correct horse", [&](bool) { ++done; }));
    CHECK(hasher.pending() == 2);
    posted.runWhen(2);
    CHECK(done == 2);
    CHECK(hasher.pending() == 0);
    CHECK(hasher.hash("correct horse", [&](auto) { ++done; }));
    posted.runWhen(1);
    CHECK(done == 3);
}

// --- Accounts ---

TEST_CASE("The schema of the master has the tables of the accounts", "[server][accounts][database]")
{
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    Fixture fixture(engine, "accounts_schema");
    CHECK(rtype::test::tableExists(fixture.database, "accounts"));
    CHECK(rtype::test::tableExists(fixture.database, "account_sessions"));
    CHECK(rtype::test::tableExists(fixture.database, "account_codes"));
    CHECK(rtype::test::tableExists(fixture.database, "discord_identities"));
}

TEST_CASE("An account is found by its id, its username or its email, ignoring case",
          "[server][accounts][database]")
{
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    Fixture fixture(engine, "accounts_find");
    Database &database = fixture.database;

    const Account created = create(database, "Pilot_7", "Pilot@Example.org");
    CHECK(created.id == 1);
    CHECK(created.username == "Pilot_7");
    CHECK(created.role == AccountRole::Player);
    CHECK_FALSE(created.verified());
    CHECK_FALSE(created.banned());

    CHECK(findAccount(database, created.id) == created);
    CHECK(findAccountByUsername(database, "pilot_7") == created);
    CHECK(findAccountByUsername(database, "PILOT_7") == created);
    CHECK(findAccountByEmail(database, " pilot@example.ORG ") == created);
    CHECK_FALSE(findAccount(database, 2));
    CHECK_FALSE(findAccountByUsername(database, "Pilot_8"));
    CHECK_FALSE(findAccountByEmail(database, "other@example.org"));
    CHECK_FALSE(findAccountByEmail(database, "not an email"));

    // Without email nor password: an account of Discord (step 11)
    auto discord = createAccount(
        database,
        {.username = "Wing", .email = std::nullopt, .passwordHash = std::nullopt, .role = AccountRole::Admin},
        Noon);
    REQUIRE(std::holds_alternative<Account>(discord));
    const Account wing = std::get<Account>(discord);
    CHECK(wing.id == 2);
    CHECK(findAccount(database, 2) == wing);
    CHECK_FALSE(wing.email);
    CHECK_FALSE(wing.passwordHash);
    CHECK(wing.role == AccountRole::Admin);
}

TEST_CASE("A Discord identity signs in to one account, which has one identity",
          "[server][accounts][database]")
{
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    Fixture fixture(engine, "accounts_discord");
    Database &database = fixture.database;
    const Account pilot = create(database, "Pilot", "pilot@example.org");
    markEmailVerified(database, pilot.id, Noon);

    // A new identity: an account of its own, without email nor password, never purged
    using Result = std::variant<Account, AccountConflict>;
    const Result created = createDiscordAccount(database, "Wing", "111", Noon);
    REQUIRE(std::holds_alternative<Account>(created));
    const Account wing = std::get<Account>(created);
    CHECK_FALSE(wing.email);
    CHECK_FALSE(wing.passwordHash);
    CHECK(findAccountByDiscord(database, "111") == wing);
    CHECK(discordIdentity(database, wing.id) == "111");
    CHECK(purgeUnverifiedAccounts(database, Noon + UnverifiedAccountLifetime * 2) == 0);
    // The identity has its account, and the username is taken ignoring case
    CHECK(createDiscordAccount(database, "Other", "111", Noon) == Result(AccountConflict::EmailTaken));
    CHECK(createDiscordAccount(database, "WING", "222", Noon) == Result(AccountConflict::UsernameTaken));
    CHECK_FALSE(findAccountByDiscord(database, "222"));
    CHECK_FALSE(findAccountByUsername(database, "Other"));

    // Linked to an account with a password, then replaced by another identity
    CHECK(linkDiscord(database, pilot.id, "222", Noon) == DiscordLink::Linked);
    CHECK(linkDiscord(database, pilot.id, "222", Noon) == DiscordLink::AlreadyLinked);
    CHECK(linkDiscord(database, pilot.id, "111", Noon) == DiscordLink::Taken);
    CHECK(linkDiscord(database, 99, "333", Noon) == DiscordLink::NoAccount);
    CHECK(findAccountByDiscord(database, "222")->id == pilot.id);
    CHECK(linkDiscord(database, pilot.id, "333", Noon) == DiscordLink::Linked);
    CHECK_FALSE(findAccountByDiscord(database, "222"));
    CHECK(discordIdentity(database, pilot.id) == "333");

    // Deleted with its account: the identity can make a new one
    CHECK(deleteAccount(database, wing.id));
    CHECK_FALSE(findAccountByDiscord(database, "111"));
    CHECK_FALSE(discordIdentity(database, wing.id));
    CHECK(std::holds_alternative<Account>(createDiscordAccount(database, "Wing", "111", Noon)));
}

TEST_CASE(
    "The panel lists the accounts newest first, by username, email or Discord id, and changes their role",
    "[server][accounts][database]")
{
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    Fixture fixture(engine, "accounts_listed");
    Database &database = fixture.database;
    const Account pilot = create(database, "Pilot", "pilot@example.org");
    const Account wing = create(database, "Wing_Man", "WING@example.org");
    const Account ace = create(database, "Ace", "ace@other.org");
    CHECK(linkDiscord(database, ace.id, "4242", Noon) == DiscordLink::Linked);

    const auto ids = [](const std::vector<AccountEntry> &entries) {
        std::vector<long long> list;
        for (const AccountEntry &entry : entries) {
            list.push_back(entry.account.id);
        }
        return list;
    };
    const std::vector<AccountEntry> all = listAccounts(database, "", 10);
    CHECK(ids(all) == std::vector<long long>{ace.id, wing.id, pilot.id});
    CHECK(all[0].discordId == "4242");
    CHECK_FALSE(all[1].discordId);
    CHECK(findAccountEntry(database, ace.id) == all[0]);
    CHECK_FALSE(findAccountEntry(database, 99));

    // Ignoring case, anywhere in the username or the email; a Discord id whole
    CHECK(ids(listAccounts(database, "wing", 10)) == std::vector<long long>{wing.id});
    CHECK(ids(listAccounts(database, "EXAMPLE.ORG", 10)) == std::vector<long long>{wing.id, pilot.id});
    CHECK(ids(listAccounts(database, "4242", 10)) == std::vector<long long>{ace.id});
    CHECK(listAccounts(database, "424", 10).empty());
    // The wildcards of LIKE are letters like the others
    CHECK(listAccounts(database, "%", 10).empty());
    CHECK(ids(listAccounts(database, "_", 10)) == std::vector<long long>{wing.id});
    // Pages
    CHECK(ids(listAccounts(database, "", 1)) == std::vector<long long>{ace.id});
    CHECK(ids(listAccounts(database, "", 10, wing.id)) == std::vector<long long>{pilot.id});

    CHECK(parseRole("admin") == AccountRole::Admin);
    CHECK(parseRole("player") == AccountRole::Player);
    CHECK_FALSE(parseRole("Admin"));
    CHECK(setRole(database, pilot.id, AccountRole::Admin));
    CHECK(findAccount(database, pilot.id)->role == AccountRole::Admin);
    CHECK(setRole(database, pilot.id, AccountRole::Player));
    CHECK(findAccount(database, pilot.id)->role == AccountRole::Player);
    CHECK_FALSE(setRole(database, 99, AccountRole::Admin));
}

TEST_CASE("Usernames and emails are unique ignoring case", "[server][accounts][database]")
{
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    Fixture fixture(engine, "accounts_unique");
    Database &database = fixture.database;
    create(database, "Pilot", "pilot@example.org");

    using Result = std::variant<Account, AccountConflict>;
    CHECK(createAccount(database,
                        {.username = "PILOT",
                         .email = "other@example.org",
                         .passwordHash = SomeHash,
                         .role = AccountRole::Player},
                        Noon) == Result(AccountConflict::UsernameTaken));
    CHECK(createAccount(database,
                        {.username = "Other",
                         .email = "Pilot@Example.ORG",
                         .passwordHash = SomeHash,
                         .role = AccountRole::Player},
                        Noon) == Result(AccountConflict::EmailTaken));
    // The username is checked first
    CHECK(createAccount(database,
                        {.username = "pilot",
                         .email = "PILOT@example.org",
                         .passwordHash = SomeHash,
                         .role = AccountRole::Player},
                        Noon) == Result(AccountConflict::UsernameTaken));
    // Several accounts without email
    create(database, "Wing", std::nullopt);
    create(database, "Wing2", std::nullopt);
    long long count = 0;
    database.session() << "SELECT COUNT(*) FROM accounts", soci::into(count);
    CHECK(count == 3);
}

TEST_CASE("An account is created, signed in, and resumed from its token alone",
          "[server][accounts][database]")
{
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    Fixture fixture(engine, "accounts_sign_in");
    Database &database = fixture.database;

    const std::string hash = hashPassword("correct horse", CheapParameters);
    auto created = createAccount(database,
                                 {.username = "Pilot",
                                  .email = "pilot@example.org",
                                  .passwordHash = hash,
                                  .role = AccountRole::Player},
                                 Noon);
    REQUIRE(std::holds_alternative<Account>(created));
    const std::string code = createCode(database, std::get<Account>(created).id, CodePurpose::Verify, Noon);
    REQUIRE(checkCode(database, std::get<Account>(created).id, CodePurpose::Verify, code, Noon) ==
            CodeCheck::Accepted);
    REQUIRE(markEmailVerified(database, std::get<Account>(created).id, Noon + 1min));

    // Signing in: found by its email, its password checked
    const std::optional<Account> found = findAccountByEmail(database, "pilot@example.org");
    REQUIRE(found);
    CHECK(found->verified());
    CHECK(found->emailVerifiedAt == Noon + 1min);
    REQUIRE(found->passwordHash);
    REQUIRE(verifyPassword(*found->passwordHash, "correct horse"));
    const SessionToken token = createSession(database, found->id, SessionClient::Game, Noon + 2min);
    CHECK(findAccount(database, found->id)->lastSeenAt == Noon + 2min);

    // Another connection, a day later: the token alone
    const std::optional<Account> resumed = resumeSession(database, token, SessionClient::Game, Noon + 24h);
    REQUIRE(resumed);
    CHECK(resumed->id == found->id);
    CHECK(resumed->username == "Pilot");
    CHECK(resumed->lastSeenAt == Noon + 24h);
    CHECK(findAccount(database, found->id)->lastSeenAt == Noon + 24h);

    // The database keeps the hash of the token, not the token
    std::string kept;
    database.session() << "SELECT token_hash FROM account_sessions", soci::into(kept);
    CHECK(kept.size() == 64);
    CHECK(kept != formatSecret(token));
}

TEST_CASE("A session expires after its lifetime without use, each use pushing it back",
          "[server][accounts][database]")
{
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    Fixture fixture(engine, "accounts_session_expiry");
    Database &database = fixture.database;
    const Account account = create(database, "Pilot", "pilot@example.org");

    const SessionToken token = createSession(database, account.id, SessionClient::Game, Noon);
    // Used after 29 days: 30 days more from then
    REQUIRE(resumeSession(database, token, SessionClient::Game, Noon + std::chrono::days(29)));
    REQUIRE(resumeSession(database, token, SessionClient::Game, Noon + std::chrono::days(58)));
    // Then left 30 days: gone, and deleted
    CHECK_FALSE(resumeSession(database, token, SessionClient::Game, Noon + std::chrono::days(88)));
    long long sessions = 0;
    database.session() << "SELECT COUNT(*) FROM account_sessions", soci::into(sessions);
    CHECK(sessions == 0);

    // A session of the panel lasts 12 hours, and is not one of the game
    const SessionToken panel = createSession(database, account.id, SessionClient::Panel, Noon);
    CHECK_FALSE(resumeSession(database, panel, SessionClient::Game, Noon + 1h));
    CHECK(resumeSession(database, panel, SessionClient::Panel, Noon + 11h));
    CHECK_FALSE(resumeSession(database, panel, SessionClient::Panel, Noon + 23h));

    // A token never given
    CHECK_FALSE(resumeSession(database, SessionToken{}, SessionClient::Game, Noon));
}

TEST_CASE("Signing out ends one session; a new password or a ban ends them all",
          "[server][accounts][database]")
{
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    Fixture fixture(engine, "accounts_end_sessions");
    Database &database = fixture.database;
    const Account account = create(database, "Pilot", "pilot@example.org");
    const Account other = create(database, "Wing", "wing@example.org");

    const SessionToken laptop = createSession(database, account.id, SessionClient::Game, Noon);
    const SessionToken desktop = createSession(database, account.id, SessionClient::Game, Noon);
    const SessionToken wing = createSession(database, other.id, SessionClient::Game, Noon);
    CHECK(laptop != desktop);

    // Signing out on the laptop
    CHECK(endSession(database, laptop));
    CHECK_FALSE(endSession(database, laptop));
    CHECK_FALSE(resumeSession(database, laptop, SessionClient::Game, Noon));
    CHECK(resumeSession(database, desktop, SessionClient::Game, Noon));

    // A new password: every session of the account ends, not those of others
    const SessionToken phone = createSession(database, account.id, SessionClient::Game, Noon);
    CHECK(setPassword(database, account.id, "$argon2id$v=19$m=8,t=1,p=1$bmV3c2FsdA$bmV3"));
    CHECK(findAccount(database, account.id)->passwordHash == "$argon2id$v=19$m=8,t=1,p=1$bmV3c2FsdA$bmV3");
    CHECK_FALSE(resumeSession(database, desktop, SessionClient::Game, Noon));
    CHECK_FALSE(resumeSession(database, phone, SessionClient::Game, Noon));
    CHECK(resumeSession(database, wing, SessionClient::Game, Noon));
    CHECK_FALSE(setPassword(database, 99, SomeHash));

    // A ban: every session ends, and none resumes until the account is unbanned
    const SessionToken before = createSession(database, other.id, SessionClient::Game, Noon);
    CHECK(banAccount(database, other.id, "cheating", Noon + 1h));
    const std::optional<Account> banned = findAccount(database, other.id);
    CHECK(banned->banned());
    CHECK(banned->bannedAt == Noon + 1h);
    CHECK(banned->banReason == "cheating");
    CHECK_FALSE(resumeSession(database, wing, SessionClient::Game, Noon + 1h));
    CHECK_FALSE(resumeSession(database, before, SessionClient::Game, Noon + 1h));
    const SessionToken during = createSession(database, other.id, SessionClient::Game, Noon + 1h);
    CHECK_FALSE(resumeSession(database, during, SessionClient::Game, Noon + 1h));
    CHECK(unbanAccount(database, other.id));
    CHECK_FALSE(findAccount(database, other.id)->banned());
    CHECK(findAccount(database, other.id)->banReason.empty());
    CHECK(resumeSession(database, during, SessionClient::Game, Noon + 2h));
    CHECK_FALSE(banAccount(database, 99, "", Noon));

    CHECK(endAllSessions(database, other.id) == 1);
}

// --- Codes ---

TEST_CASE("A code is used once, for its account and its purpose", "[server][accounts][database]")
{
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    Fixture fixture(engine, "accounts_code_once");
    Database &database = fixture.database;
    const Account account = create(database, "Pilot", "pilot@example.org");
    const Account other = create(database, "Wing", "wing@example.org");

    const std::string code = createCode(database, account.id, CodePurpose::Verify, Noon);
    CHECK(code.size() == 6);
    CHECK(code.find_first_not_of("0123456789") == std::string::npos);
    // Not the code of another purpose, nor of another account
    CHECK(checkCode(database, account.id, CodePurpose::Reset, code, Noon) == CodeCheck::NoCode);
    CHECK(checkCode(database, other.id, CodePurpose::Verify, code, Noon) == CodeCheck::NoCode);
    CHECK(checkCode(database, 99, CodePurpose::Verify, code, Noon) == CodeCheck::NoCode);

    CHECK(checkCode(database, account.id, CodePurpose::Verify, code, Noon + 1min) == CodeCheck::Accepted);
    CHECK(checkCode(database, account.id, CodePurpose::Verify, code, Noon + 1min) == CodeCheck::NoCode);

    // A new code replaces the one before it
    const std::string first = createCode(database, account.id, CodePurpose::Reset, Noon);
    std::string second = createCode(database, account.id, CodePurpose::Reset, Noon);
    while (second == first) {
        second = createCode(database, account.id, CodePurpose::Reset, Noon);
    }
    CHECK(checkCode(database, account.id, CodePurpose::Reset, first, Noon) == CodeCheck::Wrong);
    CHECK(checkCode(database, account.id, CodePurpose::Reset, second, Noon) == CodeCheck::Accepted);

    // The database keeps its hash, not the code
    const std::string third = createCode(database, account.id, CodePurpose::Reset, Noon);
    std::string kept;
    database.session() << "SELECT code_hash FROM account_codes", soci::into(kept);
    CHECK(kept.size() == 64);
    CHECK(kept.find(third) == std::string::npos);
}

TEST_CASE("A code expires after 15 minutes", "[server][accounts][database]")
{
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    Fixture fixture(engine, "accounts_code_expiry");
    Database &database = fixture.database;
    const Account account = create(database, "Pilot", "pilot@example.org");

    const std::string code = createCode(database, account.id, CodePurpose::Verify, Noon);
    CHECK(checkCode(database, account.id, CodePurpose::Verify, otherCode(code), Noon + 14min) ==
          CodeCheck::Wrong);
    CHECK(checkCode(database, account.id, CodePurpose::Verify, code, Noon + 15min) == CodeCheck::Expired);
    CHECK(checkCode(database, account.id, CodePurpose::Verify, code, Noon + 15min) == CodeCheck::NoCode);
}

TEST_CASE("A code is refused after 5 wrong attempts, even right", "[server][accounts][database]")
{
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    Fixture fixture(engine, "accounts_code_attempts");
    Database &database = fixture.database;
    const Account account = create(database, "Pilot", "pilot@example.org");

    const std::string code = createCode(database, account.id, CodePurpose::Verify, Noon);
    for (int attempt = 0; attempt < MaxCodeAttempts; ++attempt) {
        CHECK(checkCode(database, account.id, CodePurpose::Verify, otherCode(code), Noon) ==
              CodeCheck::Wrong);
    }
    CHECK(checkCode(database, account.id, CodePurpose::Verify, code, Noon) == CodeCheck::TooManyAttempts);
    CHECK(checkCode(database, account.id, CodePurpose::Verify, "not a code", Noon) ==
          CodeCheck::TooManyAttempts);

    // A new code starts again
    const std::string fresh = createCode(database, account.id, CodePurpose::Verify, Noon);
    CHECK(checkCode(database, account.id, CodePurpose::Verify, fresh, Noon) == CodeCheck::Accepted);
}

TEST_CASE("20 wrong attempts in a day, across several codes, stop every code until the next day",
          "[server][accounts][database]")
{
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    Fixture fixture(engine, "accounts_code_daily");
    Database &database = fixture.database;
    const Account account = create(database, "Pilot", "pilot@example.org");
    const Account other = create(database, "Wing", "wing@example.org");

    // 4 codes of 5 wrong attempts each, of both purposes: the 20th reaches the limit
    std::vector<CodeCheck> results;
    for (int round = 0; round < 4; ++round) {
        const CodePurpose purpose = round % 2 == 0 ? CodePurpose::Verify : CodePurpose::Reset;
        const std::string code =
            createCode(database, account.id, purpose, Noon + std::chrono::minutes(round));
        for (int attempt = 0; attempt < MaxCodeAttempts; ++attempt) {
            results.push_back(checkCode(database, account.id, purpose, otherCode(code),
                                        Noon + std::chrono::minutes(round)));
        }
    }
    REQUIRE(results.size() == static_cast<std::size_t>(MaxDailyCodeFailures));
    CHECK(std::ranges::count(results, CodeCheck::Wrong) == MaxDailyCodeFailures - 1);
    CHECK(results.back() == CodeCheck::DailyLimitReached);

    // A new code, right, is not even checked
    const std::string code = createCode(database, account.id, CodePurpose::Verify, Noon + 1h);
    CHECK(checkCode(database, account.id, CodePurpose::Verify, code, Noon + 1h) == CodeCheck::DailyLimit);
    // Other accounts are not stopped
    const std::string theirs = createCode(database, other.id, CodePurpose::Verify, Noon + 1h);
    CHECK(checkCode(database, other.id, CodePurpose::Verify, theirs, Noon + 1h) == CodeCheck::Accepted);

    // The next day (UTC), the count starts again
    const TimePoint tomorrow = Noon + 12h;
    const std::string next = createCode(database, account.id, CodePurpose::Verify, tomorrow);
    CHECK(checkCode(database, account.id, CodePurpose::Verify, otherCode(next), tomorrow) ==
          CodeCheck::Wrong);
    CHECK(checkCode(database, account.id, CodePurpose::Verify, next, tomorrow) == CodeCheck::Accepted);
}

// --- Purge ---

TEST_CASE("Accounts not verified within 24 hours are purged, with their codes and sessions",
          "[server][accounts][database]")
{
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    Fixture fixture(engine, "accounts_purge");
    Database &database = fixture.database;

    const Account old = create(database, "Old", "old@example.org", Noon);
    const Account recent = create(database, "Recent", "recent@example.org", Noon + 2h);
    const Account verified = create(database, "Verified", "verified@example.org", Noon);
    REQUIRE(markEmailVerified(database, verified.id, Noon + 1h));
    // Made with Discord, without email: never verified, never purged
    const Account discord = create(database, "Discord", std::nullopt, Noon);
    static_cast<void>(createCode(database, old.id, CodePurpose::Verify, Noon));
    static_cast<void>(createSession(database, old.id, SessionClient::Game, Noon));

    CHECK(purgeUnverifiedAccounts(database, Noon + 23h) == 0);
    CHECK(purgeUnverifiedAccounts(database, Noon + 24h) == 1);
    CHECK_FALSE(findAccount(database, old.id));
    CHECK(findAccount(database, recent.id));
    CHECK(findAccount(database, verified.id));
    CHECK(findAccount(database, discord.id));
    long long codes = 0;
    long long sessions = 0;
    database.session() << "SELECT COUNT(*) FROM account_codes", soci::into(codes);
    database.session() << "SELECT COUNT(*) FROM account_sessions", soci::into(sessions);
    CHECK(codes == 0);
    CHECK(sessions == 0);

    // Its username and email are free again
    create(database, "old", "OLD@example.org", Noon + 24h);
    CHECK(purgeUnverifiedAccounts(database, Noon + 26h) == 1);
    CHECK_FALSE(findAccount(database, recent.id));
}

TEST_CASE("The progression of an account adds up its games, keeps its records, and goes with it",
          "[server][accounts][database]")
{
    const auto engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    Fixture fixture(engine, "accounts_progress");
    Database &database = fixture.database;
    const Account pilot = create(database, "Pilot", "pilot@example.org");
    const Account other = create(database, "Other", "other@example.org");

    CHECK(readProgress(database, pilot.id) == AccountProgress{});

    const AccountProgress first =
        addGame(database, pilot.id, {.xp = 300, .kills = 20, .wavesCleared = 5, .bestStreak = 4});
    CHECK(first ==
          AccountProgress{.xp = 300, .gamesPlayed = 1, .totalKills = 20, .bestWaves = 5, .bestStreak = 4});
    const AccountProgress second =
        addGame(database, pilot.id, {.xp = 150, .kills = 7, .wavesCleared = 2, .bestStreak = 6});
    CHECK(second ==
          AccountProgress{.xp = 450, .gamesPlayed = 2, .totalKills = 27, .bestWaves = 5, .bestStreak = 6});
    CHECK(readProgress(database, pilot.id) == second);
    CHECK(readProgress(database, other.id) == AccountProgress{});

    REQUIRE(deleteAccount(database, pilot.id));
    long long rows = 0;
    database.session() << "SELECT COUNT(*) FROM account_progress", soci::into(rows);
    CHECK(rows == 0);
}
