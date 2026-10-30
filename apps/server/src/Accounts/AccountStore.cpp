#include "Accounts/AccountStore.hpp"

#include "Accounts/AccountRules.hpp"
#include "Master/AdminStore.hpp"
#include "rtype/network/Crypto.hpp"

#include <soci/soci.h>

#include <array>
#include <cstdint>
#include <format>

namespace rtype::server
{

namespace
{

using TimePoint = std::chrono::system_clock::time_point;

constexpr long long MillisecondsPerDay = 24LL * 60 * 60 * 1000;

/// The columns readAccount() reads, in its order.
constexpr std::string_view AccountColumns = "id, username, email, email_verified_at, password_hash, role, "
                                            "banned_at, ban_reason, created_at, last_seen_at";

std::string_view clientName(SessionClient client)
{
    return client == SessionClient::Game ? "game" : "panel";
}

std::chrono::hours sessionLifetime(SessionClient client)
{
    return client == SessionClient::Game ? std::chrono::hours(GameSessionLifetime) : PanelSessionLifetime;
}

std::string_view purposeName(CodePurpose purpose)
{
    return purpose == CodePurpose::Verify ? "verify" : "reset";
}

std::optional<TimePoint> optionalTime(const soci::row &row, std::size_t column)
{
    if (row.get_indicator(column) == soci::i_null) {
        return std::nullopt;
    }
    return fromUnixMilliseconds(row.get<long long>(column));
}

std::optional<std::string> optionalText(const soci::row &row, std::size_t column)
{
    if (row.get_indicator(column) == soci::i_null) {
        return std::nullopt;
    }
    return row.get<std::string>(column);
}

Account readAccount(const soci::row &row)
{
    return {
        .id = row.get<long long>(0),
        .username = row.get<std::string>(1),
        .email = optionalText(row, 2),
        .emailVerifiedAt = optionalTime(row, 3),
        .passwordHash = optionalText(row, 4),
        .role = row.get<std::string>(5) == "admin" ? AccountRole::Admin : AccountRole::Player,
        .bannedAt = optionalTime(row, 6),
        .banReason = optionalText(row, 7).value_or(""),
        .createdAt = fromUnixMilliseconds(row.get<long long>(8)),
        .lastSeenAt = fromUnixMilliseconds(row.get<long long>(9)),
    };
}

/// The account of the first row the query gives, if any.
std::optional<Account> findOne(rtype::database::Database &database, const std::string &where,
                               const std::string &value)
{
    const soci::rowset<soci::row> rows =
        (database.session().prepare << std::format("SELECT {} FROM accounts WHERE {}", AccountColumns, where),
         soci::use(value));
    for (const soci::row &row : rows) {
        return readAccount(row);
    }
    return std::nullopt;
}

/// Runs a change, and tells how many rows it touched.
template <typename... Values>
std::size_t execute(rtype::database::Database &database, const std::string &query, const Values &...values)
{
    soci::statement statement = ((database.session().prepare << query), ..., soci::use(values));
    statement.execute(true);
    return static_cast<std::size_t>(statement.get_affected_rows());
}

/// The SHA-256 of a session token, in hexadecimal: the key of its row.
std::string tokenHash(const SessionToken &token)
{
    return formatSecret(rtype::network::sha256(std::span<const std::byte>(token)));
}

/// The hash of a code, bound to its account and its purpose: the same code of another account or purpose
/// does not give the same hash.
rtype::network::Sha256 codeHash(long long accountId, CodePurpose purpose, std::string_view code)
{
    return rtype::network::sha256(std::format("{}:{}:{}", accountId, purposeName(purpose), code));
}

/// 6 digits, each value as likely: the random numbers at or beyond the last whole million are drawn again.
std::string randomCode()
{
    static constexpr std::uint32_t Range = 1'000'000;
    static constexpr std::uint32_t Limit = UINT32_MAX - (UINT32_MAX % Range);
    while (true) {
        const std::array<std::byte, 4> bytes = rtype::network::randomBytes<4>();
        std::uint32_t value = 0;
        for (const std::byte byte : bytes) {
            value = (value << 8) | std::to_integer<std::uint32_t>(byte);
        }
        if (value < Limit) {
            return std::format("{:06}", value % Range);
        }
    }
}

long long dayOf(TimePoint time)
{
    return unixMilliseconds(time) / MillisecondsPerDay;
}

} // namespace

std::string_view roleName(AccountRole role)
{
    return role == AccountRole::Admin ? "admin" : "player";
}

std::variant<Account, AccountConflict> createAccount(rtype::database::Database &database,
                                                     const NewAccount &account, TimePoint now)
{
    const std::string username = usernameKey(account.username);
    const std::optional<std::string> email =
        account.email ? std::optional<std::string>(emailKey(*account.email)) : std::nullopt;
    return database.transaction([&]() -> std::variant<Account, AccountConflict> {
        if (findOne(database, "username_key = :key", username)) {
            return AccountConflict::UsernameTaken;
        }
        if (email && findOne(database, "email_key = :key", *email)) {
            return AccountConflict::EmailTaken;
        }
        const long long created = unixMilliseconds(now);
        const std::string role(roleName(account.role));
        long long id = 0;
        // One writer, the database thread: the next id cannot be taken meanwhile
        database.session()
            << "INSERT INTO accounts (id, username, username_key, email, email_key, email_verified_at, "
               "password_hash, role, banned_at, ban_reason, code_failures, code_failures_day, created_at, "
               "last_seen_at) VALUES ((SELECT COALESCE(MAX(id), 0) + 1 FROM accounts), :username, "
               ":username_key, "
               ":email, :email_key, NULL, :password_hash, :role, NULL, NULL, 0, 0, :created_at, "
               ":last_seen_at) "
               "RETURNING id",
            soci::use(account.username), soci::use(username), soci::use(account.email), soci::use(email),
            soci::use(account.passwordHash), soci::use(role), soci::use(created), soci::use(created),
            soci::into(id);
        return Account{
            .id = id,
            .username = account.username,
            .email = account.email,
            .emailVerifiedAt = std::nullopt,
            .passwordHash = account.passwordHash,
            .role = account.role,
            .bannedAt = std::nullopt,
            .banReason = "",
            .createdAt = fromUnixMilliseconds(created),
            .lastSeenAt = fromUnixMilliseconds(created),
        };
    });
}

std::optional<Account> findAccount(rtype::database::Database &database, long long id)
{
    const soci::rowset<soci::row> rows =
        (database.session().prepare << std::format("SELECT {} FROM accounts WHERE id = :id", AccountColumns),
         soci::use(id));
    for (const soci::row &row : rows) {
        return readAccount(row);
    }
    return std::nullopt;
}

std::optional<Account> findAccountByUsername(rtype::database::Database &database, std::string_view username)
{
    return findOne(database, "username_key = :key", usernameKey(username));
}

std::optional<Account> findAccountByEmail(rtype::database::Database &database, std::string_view email)
{
    const std::optional<std::string> normalized = normalizeEmail(email);
    if (!normalized) {
        return std::nullopt;
    }
    return findOne(database, "email_key = :key", emailKey(*normalized));
}

bool markEmailVerified(rtype::database::Database &database, long long accountId, TimePoint now)
{
    const long long at = unixMilliseconds(now);
    return execute(database, "UPDATE accounts SET email_verified_at = :at WHERE id = :id", at, accountId) !=
           0;
}

bool setPassword(rtype::database::Database &database, long long accountId, const std::string &passwordHash)
{
    return database.transaction([&] {
        if (execute(database, "UPDATE accounts SET password_hash = :hash WHERE id = :id", passwordHash,
                    accountId) == 0) {
            return false;
        }
        endAllSessions(database, accountId);
        return true;
    });
}

bool banAccount(rtype::database::Database &database, long long accountId, const std::string &reason,
                TimePoint now)
{
    const long long at = unixMilliseconds(now);
    return database.transaction([&] {
        if (execute(database, "UPDATE accounts SET banned_at = :at, ban_reason = :reason WHERE id = :id", at,
                    reason, accountId) == 0) {
            return false;
        }
        endAllSessions(database, accountId);
        return true;
    });
}

bool unbanAccount(rtype::database::Database &database, long long accountId)
{
    return execute(database, "UPDATE accounts SET banned_at = NULL, ban_reason = NULL WHERE id = :id",
                   accountId) != 0;
}

bool deleteAccount(rtype::database::Database &database, long long accountId)
{
    return database.transaction([&] {
        const std::optional<Account> account = findAccount(database, accountId);
        if (!account) {
            return false;
        }
        anonymizeActions(database, accountId, account->username);
        // Its codes and sessions go with it (ON DELETE CASCADE)
        execute(database, "DELETE FROM accounts WHERE id = :id", accountId);
        return true;
    });
}

std::size_t purgeUnverifiedAccounts(rtype::database::Database &database, TimePoint now)
{
    const long long limit = unixMilliseconds(now - UnverifiedAccountLifetime);
    // Their codes and sessions go with them (ON DELETE CASCADE)
    return execute(database,
                   "DELETE FROM accounts WHERE email IS NOT NULL AND email_verified_at IS NULL AND "
                   "created_at <= :limit",
                   limit);
}

SessionToken createSession(rtype::database::Database &database, long long accountId, SessionClient client,
                           TimePoint now)
{
    const SessionToken token = rtype::network::randomBytes<SecretSize>();
    const std::string hash = tokenHash(token);
    const std::string name(clientName(client));
    const long long created = unixMilliseconds(now);
    const long long expires = unixMilliseconds(now + sessionLifetime(client));
    database.transaction([&] {
        database.session()
            << "INSERT INTO account_sessions (token_hash, account_id, created_at, last_used_at, "
               "expires_at, client) VALUES (:hash, :account, :created, :used, :expires, :client)",
            soci::use(hash), soci::use(accountId), soci::use(created), soci::use(created), soci::use(expires),
            soci::use(name);
        execute(database, "UPDATE accounts SET last_seen_at = :at WHERE id = :id", created, accountId);
    });
    return token;
}

std::optional<Account> resumeSession(rtype::database::Database &database, const SessionToken &token,
                                     SessionClient client, TimePoint now)
{
    const std::string hash = tokenHash(token);
    return database.transaction([&]() -> std::optional<Account> {
        long long accountId = 0;
        long long expiresAt = 0;
        std::string name;
        database.session()
            << "SELECT account_id, expires_at, client FROM account_sessions WHERE token_hash = :hash",
            soci::use(hash), soci::into(accountId), soci::into(expiresAt), soci::into(name);
        if (!database.session().got_data() || name != clientName(client)) {
            return std::nullopt;
        }
        const long long at = unixMilliseconds(now);
        if (expiresAt <= at) {
            execute(database, "DELETE FROM account_sessions WHERE token_hash = :hash", hash);
            return std::nullopt;
        }
        std::optional<Account> account = findAccount(database, accountId);
        if (!account || account->banned()) {
            return std::nullopt;
        }
        const long long expires = unixMilliseconds(now + sessionLifetime(client));
        execute(
            database,
            "UPDATE account_sessions SET last_used_at = :at, expires_at = :expires WHERE token_hash = :hash",
            at, expires, hash);
        execute(database, "UPDATE accounts SET last_seen_at = :at WHERE id = :id", at, accountId);
        account->lastSeenAt = fromUnixMilliseconds(at);
        return account;
    });
}

bool endSession(rtype::database::Database &database, const SessionToken &token)
{
    return execute(database, "DELETE FROM account_sessions WHERE token_hash = :hash", tokenHash(token)) != 0;
}

std::size_t endAllSessions(rtype::database::Database &database, long long accountId)
{
    return execute(database, "DELETE FROM account_sessions WHERE account_id = :id", accountId);
}

std::string createCode(rtype::database::Database &database, long long accountId, CodePurpose purpose,
                       TimePoint now)
{
    std::string code = randomCode();
    const std::string hash = formatSecret(codeHash(accountId, purpose, code));
    const std::string name(purposeName(purpose));
    const long long expires = unixMilliseconds(now + CodeLifetime);
    database.session()
        << "INSERT INTO account_codes (account_id, purpose, code_hash, expires_at, attempts) "
           "VALUES (:account, :purpose, :hash, :expires, 0) ON CONFLICT (account_id, purpose) DO "
           "UPDATE SET code_hash = excluded.code_hash, expires_at = excluded.expires_at, "
           "attempts = 0",
        soci::use(accountId), soci::use(name), soci::use(hash), soci::use(expires);
    return code;
}

CodeCheck checkCode(rtype::database::Database &database, long long accountId, CodePurpose purpose,
                    std::string_view code, TimePoint now)
{
    const std::string name(purposeName(purpose));
    const long long today = dayOf(now);
    return database.transaction([&] {
        long long failures = 0;
        long long failuresDay = 0;
        database.session() << "SELECT code_failures, code_failures_day FROM accounts WHERE id = :id",
            soci::use(accountId), soci::into(failures), soci::into(failuresDay);
        if (!database.session().got_data()) {
            return CodeCheck::NoCode;
        }
        if (failuresDay != today) {
            failures = 0;
        }
        if (failures >= MaxDailyCodeFailures) {
            return CodeCheck::DailyLimit;
        }

        std::string stored;
        long long expiresAt = 0;
        long long attempts = 0;
        database.session()
            << "SELECT code_hash, expires_at, attempts FROM account_codes WHERE account_id = :id "
               "AND purpose = :purpose",
            soci::use(accountId), soci::use(name), soci::into(stored), soci::into(expiresAt),
            soci::into(attempts);
        if (!database.session().got_data()) {
            return CodeCheck::NoCode;
        }
        if (expiresAt <= unixMilliseconds(now)) {
            execute(database, "DELETE FROM account_codes WHERE account_id = :id AND purpose = :purpose",
                    accountId, name);
            return CodeCheck::Expired;
        }
        if (attempts >= MaxCodeAttempts) {
            return CodeCheck::TooManyAttempts;
        }

        const std::optional<Secret> expected = parseSecret(stored);
        if (expected && sameSecret(*expected, codeHash(accountId, purpose, code))) {
            execute(database, "DELETE FROM account_codes WHERE account_id = :id AND purpose = :purpose",
                    accountId, name);
            return CodeCheck::Accepted;
        }
        execute(
            database,
            "UPDATE account_codes SET attempts = attempts + 1 WHERE account_id = :id AND purpose = :purpose",
            accountId, name);
        ++failures;
        execute(database,
                "UPDATE accounts SET code_failures = :failures, code_failures_day = :day WHERE id = :id",
                failures, today, accountId);
        return failures >= MaxDailyCodeFailures ? CodeCheck::DailyLimitReached : CodeCheck::Wrong;
    });
}

} // namespace rtype::server
