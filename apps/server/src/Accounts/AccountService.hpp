#pragma once

#include "Accounts/AccountStore.hpp"
#include "Accounts/Discord.hpp"
#include "Accounts/Mailer.hpp"
#include "Accounts/PasswordHasher.hpp"
#include "Accounts/ProgressStore.hpp"
#include "rtype/database/DatabaseThread.hpp"
#include "rtype/lobby/Messages.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/Timer.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>

namespace rtype::server
{

/// The limits of the accounts against guessing and floods (docs/architecture/accounts.md#the-limits). The
/// defaults are those of the protocol; the tests use smaller ones.
struct AccountLimits {
    /// Requests of the accounts from one IP address in `addressWindow`; beyond, TOO_MANY_ATTEMPTS.
    std::size_t requestsPerAddress = 10;
    std::chrono::milliseconds addressWindow = std::chrono::minutes(1);
    /// Wrong passwords of one account in a row, after which it is paused: TOO_MANY_ATTEMPTS, its password not
    /// checked, for `wrongPasswordPause`.
    unsigned wrongPasswords = 10;
    std::chrono::milliseconds wrongPasswordPause = std::chrono::minutes(15);
    /// RESEND_CODE for one login, at most once in this time.
    std::chrono::milliseconds resendInterval = std::chrono::minutes(1);
    /// The accounts not verified are purged at start, then at this interval.
    std::chrono::milliseconds purgeInterval = std::chrono::hours(1);
    /// A DISCORD_SIGN_IN is valid this long for its DISCORD_CONFIRM, and a new Discord user confirmed this
    /// long for its CHOOSE_USERNAME.
    std::chrono::milliseconds discordLifetime = std::chrono::minutes(10);
    /// Codes Discord refuses after one DISCORD_SIGN_IN: beyond, TOO_MANY_ATTEMPTS until the next.
    unsigned discordAttempts = 5;
    /// The account's own Discord identity confirmed is valid this long for DELETE_ACCOUNT without password.
    std::chrono::milliseconds deletionConfirmation = std::chrono::minutes(5);
};

struct AccountServiceSettings {
    /// `accounts.guests`: whether players can play without an account.
    bool guests = true;
    AccountLimits limits;
};

/// An account signed in on a connection, and the token of its session.
struct SignedInAccount {
    rtype::lobby::AccountInfo account;
    /// Secret: never logged.
    SessionToken token{};
};

/// Where a connection stands with Discord (docs/architecture/accounts.md#discord): kept by the lobby for each
/// connection, read and written by the AccountService, on the loop.
struct DiscordProgress {
    using Time = std::chrono::steady_clock::time_point;

    /// The last DISCORD_SIGN_IN not used yet by a DISCORD_CONFIRM, and the codes Discord refused since.
    std::optional<Time> startedAt;
    unsigned refused = 0;
    /// A Discord user without account, confirmed on this connection: CHOOSE_USERNAME makes its account.
    std::optional<DiscordUser> newUser;
    Time newUserAt;
    /// Signed in, the account's own Discord identity confirmed: DELETE_ACCOUNT needs no password.
    std::optional<Time> confirmedAt;
};

/// What a request of the accounts needs to know of its connection.
struct AccountConnection {
    rtype::network::Address address;
    /// From HELLO: the language of the mails.
    std::string locale;
    std::optional<SignedInAccount> signedIn;
    /// Never null: the Discord requests of this connection change it.
    std::shared_ptr<DiscordProgress> discord = std::make_shared<DiscordProgress>();
};

/// The reply CODE_SENT.
struct CodeSent {};
/// The reply DONE. After SIGN_OUT and DELETE_ACCOUNT, the connection is signed out and closed.
struct AccountDone {
    bool closes = false;
};

/// What a request of the accounts gets: ERROR with its code, SIGNED_IN (the connection is now signed in to
/// that account), CODE_SENT, DONE, DISCORD_AUTHORIZE, PROFILE or AVATAR.
using AccountReply = std::variant<rtype::lobby::ErrorCode, SignedInAccount, CodeSent, AccountDone,
                                  rtype::lobby::DiscordAuthorizeMessage, rtype::lobby::ProfileMessage,
                                  rtype::lobby::AvatarMessage>;

/// The accounts of the lobby (docs/architecture/accounts.md#in-the-lobby): the requests REGISTER to
/// DELETE_ACCOUNT, the sessions resumed by HELLO, the names of guests, on the thread of the loop.
///
/// @code
/// AccountService accounts(loop, databaseThread, hasher, mailer, {.guests = true}, &discord);
/// lobby.setAccounts(&accounts);
/// @endcode
///
/// - **Asynchronous**: each request goes through the database thread, the hasher and the mailer, and its
///   reply comes back through `done`, always later, on the loop, never inside the call. The lobby answers one
///   request of a connection at a time, the next ones waiting.
/// - **Discord**, with a DiscordClient: DISCORD_SIGN_IN gives the application, DISCORD_CONFIRM the code the
///   client got from Discord, which the DiscordClient exchanges for the user. Only DISCORD_CONFIRM signs in
///   or links anything; a new user chooses its username with CHOOSE_USERNAME.
/// - **Nothing tells the accounts of others**: REGISTER answers CODE_SENT whether the email has an account or
///   not, FORGOT_PASSWORD and RESEND_CODE too, a wrong login and a wrong password get the same error after
///   the same work, and EMAIL_NOT_VERIFIED comes only after the right password.
/// - **The profiles**: GET_PROFILE gives the Discord profile of an account, GET_AVATAR its picture in parts.
///   They only read, and do not count against the limits of an address.
/// - **The limits** of AccountLimits, kept in memory: per address, per account, per login.
/// - **The purge** of the accounts not verified within 24 hours, at start then every hour.
/// - **Destroying it** drops the replies not given yet: their `done` is never called.
///
/// No password, code nor token reaches a log.
class AccountService
{
public:
    using Done = std::function<void(AccountReply)>;
    using ProgressDone = std::function<void(std::optional<AccountProgress>)>;

private:
    using SteadyClock = std::chrono::steady_clock;

    /// The wrong passwords of an account in a row.
    struct Strikes {
        unsigned count = 0;
        SteadyClock::time_point pausedUntil;
    };

    rtype::network::EventLoop &_loop;
    rtype::database::DatabaseThread &_database;
    PasswordHasher &_hasher;
    Mailer &_mailer;
    DiscordClient *_discord;
    AccountServiceSettings _settings;
    /// Checked against a password when the login has no account, so that it takes as long.
    std::string _dummyHash;
    /// The recent requests of each IP address, oldest first.
    std::unordered_map<rtype::network::Address, std::deque<SteadyClock::time_point>> _requests;
    std::map<long long, Strikes> _strikes;
    /// When each login, by its key, last asked RESEND_CODE.
    std::map<std::string, SteadyClock::time_point> _resent;
    rtype::network::Timer _purge;
    /// Dies with the service: what comes back to the loop checks it.
    std::shared_ptr<bool> _alive = std::make_shared<bool>(true);

    /// Calls `done` later, on the loop.
    void later(const Done &done, AccountReply reply);
    /// Runs `task` on the database thread, then `done` on the loop with the progression it gives; nothing
    /// if the database thread is full or the task failed.
    template <typename Task>
    void onProgress(std::string description, Task task, ProgressDone done);
    /// Runs `task` on the database thread, then `then` on the loop with its result; BUSY if the database
    /// thread is full or the task failed.
    template <typename Task, typename Then>
    void onDatabase(std::string description, Task task, const Done &done, Then then);
    /// Hashes a password, then `then` on the loop with its hash; BUSY if the hasher is full or failed.
    void hash(std::string password, const Done &done, std::function<void(std::string)> then);
    /// Checks a password, then `then` on the loop with whether it matches; BUSY if the hasher is full.
    void verify(std::string hash, std::string password, const Done &done, std::function<void(bool)> then);
    /// Sends a mail in the language of the connection.
    /// @return False if the mailer cannot take it now: MAIL_UNAVAILABLE.
    bool mail(const std::string &to, const MailContent &content);
    /// Counts a request from an address. False beyond its limit.
    bool allowAddress(const rtype::network::Address &address);
    /// Whether an account is paused, after too many wrong passwords.
    [[nodiscard]] bool paused(long long accountId) const;
    void wrongPassword(long long accountId);
    void purge();
    /// Forgets what the limits no longer need.
    void forgetOld();

    void registerAccount(const AccountConnection &connection, std::span<const std::byte> payload, Done done);
    void verifyEmail(const AccountConnection &connection, std::span<const std::byte> payload, Done done);
    void resendCode(const AccountConnection &connection, std::span<const std::byte> payload, Done done);
    void signIn(const AccountConnection &connection, std::span<const std::byte> payload, Done done);
    void signOut(const AccountConnection &connection, std::span<const std::byte> payload, Done done);
    void forgotPassword(const AccountConnection &connection, std::span<const std::byte> payload, Done done);
    void resetPassword(const AccountConnection &connection, std::span<const std::byte> payload, Done done);
    void deleteAccount(const AccountConnection &connection, std::span<const std::byte> payload, Done done);
    void discordSignIn(const AccountConnection &connection, std::span<const std::byte> payload, Done done);
    void discordConfirm(const AccountConnection &connection, std::span<const std::byte> payload, Done done);
    /// DISCORD_CONFIRM, once Discord told who the player is.
    void discordUser(const AccountConnection &connection, const DiscordUser &user, const Done &done);
    void chooseUsername(const AccountConnection &connection, std::span<const std::byte> payload, Done done);
    void getProfile(std::span<const std::byte> payload, Done done);
    void getAvatar(const AccountConnection &connection, std::span<const std::byte> payload, Done done);

public:
    /// The database thread, the hasher, the mailer and the Discord client must outlive the service; their
    /// results come back on this loop.
    /// @param discord Signing in with Discord; nullptr for none: its requests get ACCOUNTS_DISABLED.
    AccountService(rtype::network::EventLoop &loop, rtype::database::DatabaseThread &database,
                   PasswordHasher &hasher, Mailer &mailer, AccountServiceSettings settings,
                   DiscordClient *discord = nullptr);
    ~AccountService();

    AccountService(const AccountService &) = delete;
    AccountService &operator=(const AccountService &) = delete;
    AccountService(AccountService &&) = delete;
    AccountService &operator=(AccountService &&) = delete;

    /// What WELCOME offers: password accounts, guests if the settings allow them, Discord if it has a client.
    [[nodiscard]] rtype::lobby::ServerFeatures features() const;
    /// `accounts.guests`.
    [[nodiscard]] bool guests() const { return _settings.guests; }

    /// Resumes the session of the token of a HELLO: the account, SESSION_EXPIRED if the token is unknown,
    /// expired or its account banned, BUSY if the database cannot answer.
    void resume(const SessionToken &token,
                std::function<void(std::variant<rtype::lobby::AccountInfo, rtype::lobby::ErrorCode>)> done);

    /// Checks the name of a guest: nothing if it can have it, NAME_RESERVED if it is the username of an
    /// account, ignoring case, BUSY if the database cannot answer.
    void checkGuestName(const std::string &name,
                        std::function<void(std::optional<rtype::lobby::ErrorCode>)> done);

    /// Reads the progression of an account, for PROGRESS: nothing if the database cannot answer.
    void progress(long long accountId, ProgressDone done);

    /// Adds a game online to the progression of an account, then gives the progression with it: nothing if
    /// the database cannot answer, and the game is lost.
    void recordGame(long long accountId, GameResult game, ProgressDone done);

    /// Answers a request of the accounts (rtype::lobby::isAccountRequest()), its state already checked by the
    /// lobby. MALFORMED for a payload that cannot be read.
    void handle(std::uint8_t type, std::span<const std::byte> payload, const AccountConnection &connection,
                Done done);
};

} // namespace rtype::server
