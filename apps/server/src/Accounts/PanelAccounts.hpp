#pragma once

#include "Accounts/AccountStore.hpp"
#include "Accounts/Discord.hpp"
#include "Accounts/PasswordHasher.hpp"
#include "rtype/database/DatabaseThread.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"

#include <chrono>
#include <cstddef>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace rtype::server
{

/// How the administrators sign in to the panel, and how often they may try
/// (docs/architecture/web.md#accounts).
struct PanelAccountsSettings {
    /// `[admin] admins`: administrators whatever their role, by verified email or `discord:<id>`.
    std::vector<std::string> admins;
    /// `web.public_url`, without a trailing slash: Discord sends the browser back to its
    /// `/auth/discord/callback`. Empty: no Discord sign-in for the panel.
    std::string publicUrl;
    /// The page of Discord that asks the user. Changed by the tests only.
    std::string discordAuthorizeUrl = "https://discord.com/oauth2/authorize";
    /// Sign-ins from one IP address in `addressWindow`, password or Discord; beyond, refused.
    std::size_t signInsPerAddress = 10;
    std::chrono::milliseconds addressWindow = std::chrono::minutes(1);
    /// Wrong passwords of one account in a row, after which its password is not checked for
    /// `wrongPasswordPause`.
    unsigned wrongPasswords = 10;
    std::chrono::milliseconds wrongPasswordPause = std::chrono::minutes(15);
    /// A sign-in with Discord started is valid this long for its return.
    std::chrono::milliseconds discordLifetime = std::chrono::minutes(10);
    /// Sign-ins with Discord started and not returned: beyond, the oldest is forgotten.
    std::size_t maxDiscordStates = 256;
};

/// An administrator signed in to the panel.
struct PanelAccount {
    long long id = 0;
    std::string username;
    AccountRole role = AccountRole::Player;
    /// One of `[admin] admins`: the panel can neither demote, ban nor delete it.
    bool listed = false;

    friend bool operator==(const PanelAccount &, const PanelAccount &) = default;
};

/// Why the panel refuses an account.
enum class PanelRefusal {
    /// No such login, or a wrong password: the same answer for both.
    InvalidCredentials,
    /// The right password, but the email of the account is not verified.
    EmailNotVerified,
    /// Neither the role `admin` nor one of `[admin] admins`.
    NotAdmin,
    Banned,
    /// Too many sign-ins from the address, or wrong passwords of the account.
    TooManyAttempts,
    /// The database or the hasher cannot take more now.
    Busy,
    /// The session is unknown, expired, of the game, or its account is no longer an administrator.
    SessionExpired,
    /// The sign-in with Discord is unknown, used, expired, or was started by another browser.
    DiscordExpired,
    /// Discord refused the code, or the Discord user has no account.
    DiscordRefused,
    DiscordUnavailable,
};

struct PanelSignedIn {
    PanelAccount account;
    /// Secret: never logged. The value of the session cookie.
    SessionToken token{};
};

using PanelSignIn = std::variant<PanelSignedIn, PanelRefusal>;
using PanelResume = std::variant<PanelAccount, PanelRefusal>;

/// A sign-in with Discord started: where to send the browser, and what the browser keeps until it comes
/// back.
struct PanelDiscordStart {
    /// The authorization page of Discord, with the state, the redirect address and the challenge of PKCE.
    std::string url;
    /// 256 random bits in hexadecimal, which the browser keeps in a cookie and Discord gives back. Secret.
    std::string state;
};

/// The administrators of the panel (docs/architecture/web.md#accounts), on the thread of the loop: their
/// sign-in with a password or with Discord, their sessions, and who is an administrator.
///
/// @code
/// PanelAccounts panel(loop, databaseThread, hasher, &discord, {.admins = settings.admin.admins});
/// webServer.setPanelAccounts(&panel);
/// @endcode
///
/// - **An administrator** has the role `admin`, or is one of `[admin] admins`; never banned.
/// - **Its sessions** are those of `account_sessions` with the client `panel`, 12 hours after their last use;
///   resume() reads them at each request, so that a ban or a new role counts at once.
/// - **Discord**: startDiscord() makes a state, single use, valid `discordLifetime`, and a verifier of PKCE
///   kept here; finishDiscord() takes the code Discord gave back with that state, which the DiscordClient
///   exchanges for the user. The web server binds the state to the browser with a cookie.
/// - **The limits**: per address, every sign-in; per account, the wrong passwords. Kept in memory.
/// - **Destroying it** drops the answers not given yet.
///
/// No password, token, state nor code reaches a log.
class PanelAccounts
{
public:
    template <typename Result>
    using Done = std::function<void(Result)>;

private:
    using SteadyClock = std::chrono::steady_clock;

    struct Strikes {
        unsigned count = 0;
        SteadyClock::time_point pausedUntil;
    };
    struct DiscordState {
        std::string verifier;
        SteadyClock::time_point startedAt;
    };

    rtype::network::EventLoop &_loop;
    rtype::database::DatabaseThread &_database;
    PasswordHasher &_hasher;
    DiscordClient *_discord;
    PanelAccountsSettings _settings;
    std::string _dummyHash;
    std::unordered_map<rtype::network::Address, std::deque<SteadyClock::time_point>> _signIns;
    std::map<long long, Strikes> _strikes;
    /// By state.
    std::map<std::string, DiscordState> _discordStates;
    std::shared_ptr<bool> _alive = std::make_shared<bool>(true);

    /// Runs `task` on the database thread, then `then` on the loop; Busy if it cannot.
    template <typename Result, typename Task, typename Then>
    void onDatabase(std::string description, Task task, const Done<Result> &done, Then then);
    template <typename Result>
    void later(const Done<Result> &done, Result result);
    bool allowAddress(const rtype::network::Address &address);
    [[nodiscard]] bool paused(long long accountId) const;
    void wrongPassword(long long accountId);
    /// Who an account is for the panel, or why it is not let in.
    [[nodiscard]] std::variant<PanelAccount, PanelRefusal> admit(const AccountEntry &entry) const;
    /// Opens a session for an administrator.
    void openSession(const PanelAccount &account, const Done<PanelSignIn> &done);
    void forgetOld();

public:
    /// The database thread, the hasher and the Discord client must outlive it; their results come back on
    /// this loop.
    /// @param discord nullptr: no sign-in with Discord.
    PanelAccounts(rtype::network::EventLoop &loop, rtype::database::DatabaseThread &database,
                  PasswordHasher &hasher, DiscordClient *discord, PanelAccountsSettings settings);
    ~PanelAccounts();

    PanelAccounts(const PanelAccounts &) = delete;
    PanelAccounts &operator=(const PanelAccounts &) = delete;
    PanelAccounts(PanelAccounts &&) = delete;
    PanelAccounts &operator=(PanelAccounts &&) = delete;

    /// Whether the panel offers Discord: a Discord client and a public address.
    [[nodiscard]] bool discordEnabled() const;
    [[nodiscard]] const std::vector<std::string> &admins() const { return _settings.admins; }
    /// Where Discord sends the browser back: `<public_url>/auth/discord/callback`.
    [[nodiscard]] std::string discordRedirectUri() const;

    /// Signs in with a username or an email and a password, from the address of the visitor.
    void signIn(const std::string &login, std::string password, const rtype::network::Address &address,
                Done<PanelSignIn> done);
    /// Resumes the session of a cookie: the administrator, SessionExpired, or Busy.
    void resume(const SessionToken &token, Done<PanelResume> done);
    /// Ends a session; `done` gets whether it was one.
    void signOut(const SessionToken &token, Done<bool> done);

    /// Starts a sign-in with Discord from an address.
    /// @return Nothing if Discord is not enabled, or the address tried too often.
    [[nodiscard]] std::optional<PanelDiscordStart> startDiscord(const rtype::network::Address &address);
    /// The return from Discord, with the state the browser kept and the one Discord gave back, which must be
    /// the same, and the code.
    void finishDiscord(const std::string &state, const std::string &cookieState, std::string code,
                       Done<PanelSignIn> done);

    /// Gives the role `admin` to the accounts of `[admin] admins`, at start, and warns of those that have no
    /// account yet, or not a verified email. `done` gets how many accounts became administrators.
    void promoteListed(Done<std::optional<std::size_t>> done);
};

/// The CSRF token of a session of an account, drawn from its token: nothing more is kept, and a page of
/// another site, which cannot read the cookie, cannot read it either.
[[nodiscard]] Secret panelCsrfToken(const SessionToken &token);

} // namespace rtype::server
