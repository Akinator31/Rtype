#pragma once

#include "Common/Secret.hpp"
#include "rtype/database/Database.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/// What the master keeps of the accounts in its database (docs/architecture/accounts.md): the accounts
/// (`accounts`), their sessions (`account_sessions`), the codes sent by email (`account_codes`) and the
/// Discord users who sign in to them (`discord_identities`). Plain
/// functions on a Database, as Master/AdminStore.hpp: the master runs them on its database thread, the tests
/// anywhere. Each takes the time it runs at, so that the tests choose it.
///
/// The usernames and emails are expected checked by Accounts/AccountRules.hpp, and the passwords hashed by
/// Accounts/PasswordHasher.hpp: nothing here hashes a password nor checks a rule.
namespace rtype::server
{

enum class AccountRole {
    Player,
    Admin,
};

/// Where a session is used: the game, or the panel of the administrators (step 12), whose sessions are
/// shorter.
enum class SessionClient {
    Game,
    Panel,
};

/// What a code sent by email is for.
enum class CodePurpose {
    /// Verifies the email of a new account.
    Verify,
    /// Sets a new password, forgotten.
    Reset,
};

/// An account not verified within this time is deleted, with its codes.
inline constexpr std::chrono::hours UnverifiedAccountLifetime{24};
/// A session ends after this time without being used.
inline constexpr std::chrono::days GameSessionLifetime{30};
inline constexpr std::chrono::hours PanelSessionLifetime{12};
/// A code is valid this long after it is made.
inline constexpr std::chrono::minutes CodeLifetime{15};
/// Wrong tries of one code: beyond, it is refused even right.
inline constexpr int MaxCodeAttempts = 5;
/// Wrong tries of every code of an account in a day (UTC): beyond, no code is checked until the next day.
inline constexpr int MaxDailyCodeFailures = 20;

/// One account (table `accounts`).
struct Account {
    long long id = 0;
    /// As its owner wrote it; unique ignoring case.
    std::string username;
    /// None for an account made with Discord only (step 11). Unique ignoring case.
    std::optional<std::string> email;
    std::optional<std::chrono::system_clock::time_point> emailVerifiedAt;
    /// Argon2id, `$argon2id$v=19$...`; none for an account made with Discord only.
    std::optional<std::string> passwordHash;
    AccountRole role = AccountRole::Player;
    std::optional<std::chrono::system_clock::time_point> bannedAt;
    std::string banReason;
    std::chrono::system_clock::time_point createdAt;
    /// When it last signed in or resumed a session.
    std::chrono::system_clock::time_point lastSeenAt;

    [[nodiscard]] bool verified() const { return emailVerifiedAt.has_value(); }
    [[nodiscard]] bool banned() const { return bannedAt.has_value(); }

    friend bool operator==(const Account &, const Account &) = default;
};

/// What an account is made from.
struct NewAccount {
    std::string username;
    /// Normalized by normalizeEmail().
    std::optional<std::string> email;
    std::optional<std::string> passwordHash;
    AccountRole role = AccountRole::Player;
};

/// Why an account cannot be made.
enum class AccountConflict {
    /// Another account has the username, ignoring case.
    UsernameTaken,
    /// Another account has the email, ignoring case.
    EmailTaken,
};

[[nodiscard]] std::string_view roleName(AccountRole role);

/// Reads the name of a role, `player` or `admin`.
[[nodiscard]] std::optional<AccountRole> parseRole(std::string_view name);

/// Makes an account, its email not verified.
/// @return The account, or why it cannot be made: the username is checked first.
/// @throws soci::soci_error if the database refuses it.
[[nodiscard]] std::variant<Account, AccountConflict> createAccount(rtype::database::Database &database,
                                                                   const NewAccount &account,
                                                                   std::chrono::system_clock::time_point now);

/// @throws soci::soci_error if the database cannot be read.
[[nodiscard]] std::optional<Account> findAccount(rtype::database::Database &database, long long id);
/// Ignoring case.
/// @throws soci::soci_error if the database cannot be read.
[[nodiscard]] std::optional<Account> findAccountByUsername(rtype::database::Database &database,
                                                           std::string_view username);
/// Ignoring case and the spaces around it.
/// @throws soci::soci_error if the database cannot be read.
[[nodiscard]] std::optional<Account> findAccountByEmail(rtype::database::Database &database,
                                                        std::string_view email);

/// Marks the email of an account verified, once its code is accepted.
/// @return False if there is no such account.
/// @throws soci::soci_error if the database refuses it.
bool markEmailVerified(rtype::database::Database &database, long long accountId,
                       std::chrono::system_clock::time_point now);

/// Sets a new password, and ends every session of the account: whoever knew the old one is signed out.
/// @param passwordHash Made by hashPassword().
/// @return False if there is no such account.
/// @throws soci::soci_error if the database refuses it.
bool setPassword(rtype::database::Database &database, long long accountId, const std::string &passwordHash);

/// Bans an account, and ends every session of it. A banned account resumes no session.
/// @return False if there is no such account.
/// @throws soci::soci_error if the database refuses it.
bool banAccount(rtype::database::Database &database, long long accountId, const std::string &reason,
                std::chrono::system_clock::time_point now);
/// @return False if there is no such account.
/// @throws soci::soci_error if the database refuses it.
bool unbanAccount(rtype::database::Database &database, long long accountId);

/// Changes the role of an account. Its sessions stay: the panel checks the role at each request.
/// @return False if there is no such account.
/// @throws soci::soci_error if the database refuses it.
bool setRole(rtype::database::Database &database, long long accountId, AccountRole role);

/// Deletes an account, with its sessions, its codes and its Discord identity, at the request of its owner.
/// Its administration actions stay, without its name nor its address (anonymizeActions()).
/// @return False if there is no such account.
/// @throws soci::soci_error if the database refuses it.
bool deleteAccount(rtype::database::Database &database, long long accountId);

/// Deletes the accounts with an email not verified within UnverifiedAccountLifetime, with their codes and
/// sessions.
/// @return How many.
/// @throws soci::soci_error if the database refuses it.
std::size_t purgeUnverifiedAccounts(rtype::database::Database &database,
                                    std::chrono::system_clock::time_point now);

// --- Discord ---

/// What linking a Discord identity to an account gives.
enum class DiscordLink {
    /// Linked, in place of the identity the account had, if any.
    Linked,
    /// The account had this identity already.
    AlreadyLinked,
    /// The identity is linked to another account.
    Taken,
    /// There is no such account.
    NoAccount,
};

/// The account a Discord user signs in to (table `discord_identities`).
/// @param discordId The id of the user, in decimal digits.
/// @throws soci::soci_error if the database cannot be read.
[[nodiscard]] std::optional<Account> findAccountByDiscord(rtype::database::Database &database,
                                                          std::string_view discordId);

/// The id of the Discord user an account is linked to, if any.
/// @throws soci::soci_error if the database cannot be read.
[[nodiscard]] std::optional<std::string> discordIdentity(rtype::database::Database &database,
                                                         long long accountId);

/// Links a Discord user to an account: from then on, it signs in to it. An account has one identity at most:
/// a new one replaces the old.
/// @throws soci::soci_error if the database refuses it.
DiscordLink linkDiscord(rtype::database::Database &database, long long accountId,
                        const std::string &discordId, std::chrono::system_clock::time_point now);

/// Makes the account of a new Discord user, without email nor password, and links them.
/// @return The account, or why it cannot be made: UsernameTaken, or EmailTaken when the identity has an
/// account already.
/// @throws soci::soci_error if the database refuses it.
[[nodiscard]] std::variant<Account, AccountConflict>
createDiscordAccount(rtype::database::Database &database, const std::string &username,
                     const std::string &discordId, std::chrono::system_clock::time_point now);

// --- The panel ---

/// An account as the panel shows it, with the Discord user linked to it.
struct AccountEntry {
    Account account;
    std::optional<std::string> discordId;

    friend bool operator==(const AccountEntry &, const AccountEntry &) = default;
};

/// @throws soci::soci_error if the database cannot be read.
[[nodiscard]] std::optional<AccountEntry> findAccountEntry(rtype::database::Database &database,
                                                           long long accountId);

/// The accounts, newest first.
/// @param search Empty for every account; otherwise those whose username or email holds it, ignoring case,
/// or whose Discord id is it.
/// @param limit How many at most.
/// @param before Only those with a smaller id: the page after one ending at this id.
/// @throws soci::soci_error if the database cannot be read.
[[nodiscard]] std::vector<AccountEntry> listAccounts(rtype::database::Database &database,
                                                     std::string_view search, std::size_t limit,
                                                     std::optional<long long> before = std::nullopt);

// --- Sessions ---

/// What the client keeps to resume a session: 256 random bits. The database keeps only its SHA-256, so that
/// a copy of the database opens no session.
using SessionToken = Secret;

/// Opens a session for an account.
/// @return Its token, given to the client once: it cannot be read back.
/// @throws soci::soci_error if the database refuses it.
[[nodiscard]] SessionToken createSession(rtype::database::Database &database, long long accountId,
                                         SessionClient client, std::chrono::system_clock::time_point now);

/// Resumes a session from its token: its expiry moves to `now` plus its lifetime, and the account is seen.
/// @return The account, or nothing if the token is unknown, expired (it is then deleted), of another client,
/// or its account banned.
/// @throws soci::soci_error if the database refuses it.
[[nodiscard]] std::optional<Account> resumeSession(rtype::database::Database &database,
                                                   const SessionToken &token, SessionClient client,
                                                   std::chrono::system_clock::time_point now);

/// Ends a session: signing out.
/// @return False if the token is unknown.
/// @throws soci::soci_error if the database refuses it.
bool endSession(rtype::database::Database &database, const SessionToken &token);

/// Ends every session of an account.
/// @return How many.
/// @throws soci::soci_error if the database refuses it.
std::size_t endAllSessions(rtype::database::Database &database, long long accountId);

// --- Codes ---

/// Makes a code of 6 digits for an account, valid CodeLifetime, replacing the one it had for that purpose.
/// The database keeps only its hash.
/// @return The code, to send by email.
/// @throws soci::soci_error if the database refuses it.
[[nodiscard]] std::string createCode(rtype::database::Database &database, long long accountId,
                                     CodePurpose purpose, std::chrono::system_clock::time_point now);

/// What a code typed by a player gives.
enum class CodeCheck {
    /// Right: the code is used, and deleted.
    Accepted,
    /// Wrong: one attempt more, for the code and for the day of the account.
    Wrong,
    /// Wrong, and the failures of the day reach MaxDailyCodeFailures: the moment to tell the owner of the
    /// account by email.
    DailyLimitReached,
    /// The account has no code for that purpose: none made, already used, or the account is gone.
    NoCode,
    /// Too late: the code is deleted.
    Expired,
    /// The code was tried MaxCodeAttempts times: a new one is needed.
    TooManyAttempts,
    /// The account had MaxDailyCodeFailures wrong tries today: no code is checked until the next day.
    DailyLimit,
};

/// Checks a code typed by a player, in constant time.
/// @throws soci::soci_error if the database refuses it.
[[nodiscard]] CodeCheck checkCode(rtype::database::Database &database, long long accountId,
                                  CodePurpose purpose, std::string_view code,
                                  std::chrono::system_clock::time_point now);

} // namespace rtype::server
