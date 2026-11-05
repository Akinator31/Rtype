#include "Accounts/PanelAccounts.hpp"

#include "Accounts/AccountRules.hpp"
#include "Accounts/Mailer.hpp"
#include "Common/Base64.hpp"
#include "Common/Secret.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/Crypto.hpp"

#include <algorithm>
#include <format>
#include <type_traits>
#include <utility>

namespace rtype::server
{

namespace
{

using SystemClock = std::chrono::system_clock;

const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("panel");
    return instance;
}

std::optional<Account> findByLogin(rtype::database::Database &database, std::string_view login)
{
    // An email has an `@`, which no username has
    return login.find('@') != std::string_view::npos ? findAccountByEmail(database, login)
                                                     : findAccountByUsername(database, login);
}

/// What resume() and the sign-ins read: the account and its Discord user.
std::optional<AccountEntry> withDiscord(rtype::database::Database &database, std::optional<Account> account)
{
    if (!account) {
        return std::nullopt;
    }
    std::optional<std::string> discordId = discordIdentity(database, account->id);
    // Its profile without its picture, which the panel asks for on its own
    std::optional<DiscordProfile> discord = findDiscordProfile(database, account->id);
    if (discord) {
        discord->avatar.clear();
    }
    return AccountEntry{
        .account = std::move(*account), .discordId = std::move(discordId), .discord = std::move(discord)};
}

/// Encodes bytes in base64url (RFC 4648, 5) without padding, as PKCE wants its challenge.
std::string base64Url(std::string_view bytes)
{
    std::string encoded = base64(bytes);
    std::ranges::replace(encoded, '+', '-');
    std::ranges::replace(encoded, '/', '_');
    while (!encoded.empty() && encoded.back() == '=') {
        encoded.pop_back();
    }
    return encoded;
}

/// Encodes a value of a query string.
std::string escape(std::string_view value)
{
    std::string escaped;
    for (const char c : value) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
            c == '.' || c == '_' || c == '~') {
            escaped += c;
        } else {
            escaped += std::format("%{:02X}", static_cast<unsigned char>(c));
        }
    }
    return escaped;
}

std::string randomHex()
{
    return formatSecret(rtype::network::randomBytes<SecretSize>());
}

} // namespace

Secret panelCsrfToken(const SessionToken &token)
{
    return rtype::network::sha256(std::format("panel csrf:{}", formatSecret(token)));
}

PanelAccounts::PanelAccounts(rtype::network::EventLoop &loop, rtype::database::DatabaseThread &database,
                             PasswordHasher &hasher, DiscordClient *discord, PanelAccountsSettings settings)
    : _loop(loop), _database(database), _hasher(hasher), _discord(discord), _settings(std::move(settings)),
      _dummyHash(hashPassword(randomHex(), hasher.parameters()))
{
    while (_settings.publicUrl.ends_with('/')) {
        _settings.publicUrl.pop_back();
    }
}

PanelAccounts::~PanelAccounts() = default;

bool PanelAccounts::discordEnabled() const
{
    return _discord != nullptr && !_settings.publicUrl.empty();
}

std::string PanelAccounts::discordRedirectUri() const
{
    return _settings.publicUrl + "/auth/discord/callback";
}

template <typename Result>
void PanelAccounts::later(const Done<Result> &done, Result result)
{
    _loop.post([alive = std::weak_ptr<bool>(_alive), done, result = std::move(result)] {
        if (!alive.expired()) {
            done(result);
        }
    });
}

template <typename Result, typename Task, typename Then>
void PanelAccounts::onDatabase(std::string description, Task task, const Done<Result> &done, Then then)
{
    using Value = std::invoke_result_t<Task &, rtype::database::Database &>;
    const bool queued = _database.run(std::move(description), std::move(task),
                                      [alive = std::weak_ptr<bool>(_alive), done,
                                       then = std::move(then)](std::optional<Value> value) mutable {
                                          if (alive.expired()) {
                                              return;
                                          }
                                          if (!value) {
                                              done(Result(PanelRefusal::Busy));
                                              return;
                                          }
                                          then(std::move(*value));
                                      });
    if (!queued) {
        logger().warning("The database thread is full: a request of the panel is refused");
        later(done, Result(PanelRefusal::Busy));
    }
}

bool PanelAccounts::allowAddress(const rtype::network::Address &address)
{
    forgetOld();
    const auto now = SteadyClock::now();
    std::deque<SteadyClock::time_point> &times = _signIns[address.withPort(0)];
    while (!times.empty() && times.front() <= now - _settings.addressWindow) {
        times.pop_front();
    }
    if (times.size() >= _settings.signInsPerAddress) {
        return false;
    }
    times.push_back(now);
    return true;
}

bool PanelAccounts::paused(long long accountId) const
{
    const auto found = _strikes.find(accountId);
    return found != _strikes.end() && found->second.count >= _settings.wrongPasswords &&
           SteadyClock::now() < found->second.pausedUntil;
}

void PanelAccounts::wrongPassword(long long accountId)
{
    Strikes &strikes = _strikes[accountId];
    if (strikes.count >= _settings.wrongPasswords) {
        // The pause is over: counted again from the start
        strikes.count = 0;
    }
    if (++strikes.count >= _settings.wrongPasswords) {
        strikes.pausedUntil = SteadyClock::now() + _settings.wrongPasswordPause;
        logger().warning("Account {} paused on the panel after {} wrong passwords", accountId, strikes.count);
    }
}

void PanelAccounts::forgetOld()
{
    const auto now = SteadyClock::now();
    std::erase_if(_signIns, [&](const auto &entry) {
        return entry.second.empty() || entry.second.back() <= now - _settings.addressWindow;
    });
    std::erase_if(_strikes, [&](const auto &entry) {
        return entry.second.count >= _settings.wrongPasswords && entry.second.pausedUntil <= now;
    });
    std::erase_if(_discordStates, [&](const auto &entry) {
        return entry.second.startedAt <= now - _settings.discordLifetime;
    });
}

std::variant<PanelAccount, PanelRefusal> PanelAccounts::admit(const AccountEntry &entry) const
{
    const Account &account = entry.account;
    if (account.banned()) {
        return PanelRefusal::Banned;
    }
    const bool listed =
        isListedAdmin(_settings.admins, account.verified() ? account.email : std::nullopt, entry.discordId);
    if (!listed && account.role != AccountRole::Admin) {
        return PanelRefusal::NotAdmin;
    }
    return PanelAccount{
        .id = account.id,
        .username = account.username,
        .role = AccountRole::Admin,
        .listed = listed,
        .discord = entry.discord,
    };
}

void PanelAccounts::openSession(const PanelAccount &account, const Done<PanelSignIn> &done)
{
    onDatabase(
        "open a session of the panel",
        [id = account.id](rtype::database::Database &database) {
            return createSession(database, id, SessionClient::Panel, SystemClock::now());
        },
        done,
        [done, account](const SessionToken &token) {
            logger().info("{} ({}) signed in to the panel", account.username, account.id);
            done(PanelSignedIn{.account = account, .token = token});
        });
}

void PanelAccounts::signIn(const std::string &login, std::string password,
                           const rtype::network::Address &address, Done<PanelSignIn> done)
{
    if (!allowAddress(address)) {
        logger().info("{}: too many sign-ins to the panel", address.ip());
        later(done, PanelSignIn(PanelRefusal::TooManyAttempts));
        return;
    }
    onDatabase(
        "find an account to sign in to the panel",
        [login](rtype::database::Database &database) {
            return withDiscord(database, findByLogin(database, login));
        },
        done,
        [this, done, password = std::move(password)](const std::optional<AccountEntry> &entry) mutable {
            if (entry && paused(entry->account.id)) {
                done(PanelRefusal::TooManyAttempts);
                return;
            }
            // Without an account, a password is checked all the same, against nothing: the answer takes as
            // long
            const bool hasPassword = entry && entry->account.passwordHash;
            const bool queued = _hasher.verify(
                hasPassword ? *entry->account.passwordHash : _dummyHash, std::move(password),
                [this, alive = std::weak_ptr<bool>(_alive), done, entry, hasPassword](bool matches) {
                    if (alive.expired()) {
                        return;
                    }
                    if (!matches || !hasPassword) {
                        if (entry) {
                            wrongPassword(entry->account.id);
                        }
                        done(PanelRefusal::InvalidCredentials);
                        return;
                    }
                    _strikes.erase(entry->account.id);
                    if (entry->account.email && !entry->account.verified()) {
                        done(PanelRefusal::EmailNotVerified);
                        return;
                    }
                    const std::variant<PanelAccount, PanelRefusal> admitted = admit(*entry);
                    if (const auto *refusal = std::get_if<PanelRefusal>(&admitted)) {
                        logger().info("Account {} refused by the panel: {}", entry->account.id,
                                      *refusal == PanelRefusal::Banned ? "banned" : "not an administrator");
                        done(*refusal);
                        return;
                    }
                    openSession(std::get<PanelAccount>(admitted), done);
                });
            if (!queued) {
                done(PanelRefusal::Busy);
            }
        });
}

void PanelAccounts::resume(const SessionToken &token, Done<PanelResume> done)
{
    onDatabase(
        "resume a session of the panel",
        [token](rtype::database::Database &database) {
            return withDiscord(database,
                               resumeSession(database, token, SessionClient::Panel, SystemClock::now()));
        },
        done,
        [this, done](const std::optional<AccountEntry> &entry) {
            if (!entry) {
                done(PanelRefusal::SessionExpired);
                return;
            }
            const std::variant<PanelAccount, PanelRefusal> admitted = admit(*entry);
            if (const auto *account = std::get_if<PanelAccount>(&admitted)) {
                done(*account);
            } else {
                // No longer an administrator: its session opens nothing any more
                done(PanelRefusal::SessionExpired);
            }
        });
}

void PanelAccounts::signOut(const SessionToken &token, Done<bool> done)
{
    const bool queued = _database.run(
        "end a session of the panel",
        [token](rtype::database::Database &database) { return endSession(database, token); },
        [alive = std::weak_ptr<bool>(_alive), done](std::optional<bool> ended) {
            if (!alive.expired()) {
                done(ended.value_or(false));
            }
        });
    if (!queued) {
        later(done, false);
    }
}

std::optional<PanelDiscordStart> PanelAccounts::startDiscord(const rtype::network::Address &address)
{
    if (!discordEnabled()) {
        return std::nullopt;
    }
    if (!allowAddress(address)) {
        logger().info("{}: too many sign-ins to the panel", address.ip());
        return std::nullopt;
    }
    if (_discordStates.size() >= _settings.maxDiscordStates) {
        const auto oldest = std::ranges::min_element(_discordStates, [](const auto &a, const auto &b) {
            return a.second.startedAt < b.second.startedAt;
        });
        _discordStates.erase(oldest);
    }
    std::string state = randomHex();
    std::string verifier = randomHex();
    const rtype::network::Sha256 digest = rtype::network::sha256(verifier);
    const std::string challenge =
        base64Url(std::string_view(reinterpret_cast<const char *>(digest.data()), digest.size()));
    const std::string url =
        std::format("{}?response_type=code&client_id={}&scope=identify&state={}&redirect_uri={}"
                    "&code_challenge={}&code_challenge_method=S256",
                    _settings.discordAuthorizeUrl, _discord->applicationId(), state,
                    escape(discordRedirectUri()), challenge);
    _discordStates.emplace(state,
                           DiscordState{.verifier = std::move(verifier), .startedAt = SteadyClock::now()});
    return PanelDiscordStart{.url = url, .state = std::move(state)};
}

void PanelAccounts::finishDiscord(const std::string &state, const std::string &cookieState, std::string code,
                                  Done<PanelSignIn> done)
{
    forgetOld();
    const std::optional<Secret> given = parseSecret(state);
    const std::optional<Secret> kept = parseSecret(cookieState);
    const auto found = given ? _discordStates.find(state) : _discordStates.end();
    // The state Discord gave back must be the one of this browser: a link of another opens nothing
    if (_discord == nullptr || !kept || found == _discordStates.end() || !sameSecret(*given, *kept)) {
        logger().info(
            "A return from Discord to the panel was refused: its state is unknown, used, expired, or "
            "of another browser");
        later(done, PanelSignIn(PanelRefusal::DiscordExpired));
        return;
    }
    // Single use, whatever comes next
    const std::string verifier = std::move(found->second.verifier);
    _discordStates.erase(found);
    const bool queued = _discord->identify(
        DiscordAuthorization{
            .code = std::move(code), .codeVerifier = verifier, .redirectUri = discordRedirectUri()},
        [this, alive = std::weak_ptr<bool>(_alive), done](const DiscordAnswer &answer) {
            if (alive.expired()) {
                return;
            }
            if (const auto *failure = std::get_if<DiscordFailure>(&answer)) {
                done(*failure == DiscordFailure::Refused ? PanelRefusal::DiscordRefused
                                                         : PanelRefusal::DiscordUnavailable);
                return;
            }
            const DiscordUser user = std::get<DiscordUser>(answer);
            onDatabase(
                "find the account of a Discord user for the panel",
                [user](rtype::database::Database &database) {
                    std::optional<Account> account = findAccountByDiscord(database, user.id);
                    if (account) {
                        saveDiscordProfile(database, user.id, profileOf(user), SystemClock::now());
                    }
                    return withDiscord(database, std::move(account));
                },
                done,
                [this, done](const std::optional<AccountEntry> &entry) {
                    if (!entry) {
                        logger().info("A Discord user without account was refused by the panel");
                        done(PanelRefusal::DiscordRefused);
                        return;
                    }
                    const std::variant<PanelAccount, PanelRefusal> admitted = admit(*entry);
                    if (const auto *refusal = std::get_if<PanelRefusal>(&admitted)) {
                        logger().info("Account {} refused by the panel: {}", entry->account.id,
                                      *refusal == PanelRefusal::Banned ? "banned" : "not an administrator");
                        done(*refusal);
                        return;
                    }
                    openSession(std::get<PanelAccount>(admitted), done);
                });
        });
    if (!queued) {
        later(done, PanelSignIn(PanelRefusal::Busy));
    }
}

void PanelAccounts::promoteListed(Done<std::optional<std::size_t>> done)
{
    struct Outcome {
        std::size_t promoted = 0;
        std::vector<std::string> missing;
    };
    const bool queued = _database.run(
        "give the role admin to [admin] admins",
        [admins = _settings.admins](rtype::database::Database &database) {
            Outcome outcome;
            for (const std::string &admin : admins) {
                std::optional<Account> account = admin.starts_with("discord:")
                                                     ? findAccountByDiscord(database, admin.substr(8))
                                                     : findAccountByEmail(database, admin);
                const std::optional<AccountEntry> entry = withDiscord(database, std::move(account));
                if (!entry ||
                    !isListedAdmin({admin}, entry->account.verified() ? entry->account.email : std::nullopt,
                                   entry->discordId)) {
                    outcome.missing.push_back(admin);
                    continue;
                }
                if (entry->account.role != AccountRole::Admin) {
                    setRole(database, entry->account.id, AccountRole::Admin);
                    ++outcome.promoted;
                }
            }
            return outcome;
        },
        [alive = std::weak_ptr<bool>(_alive), done](std::optional<Outcome> outcome) {
            if (alive.expired()) {
                return;
            }
            if (!outcome) {
                logger().error("Cannot give the role admin to [admin] admins: the database refused it");
                done(std::nullopt);
                return;
            }
            for (const std::string &admin : outcome->missing) {
                logger().warning(
                    "[admin] admins: {} has no account yet, or not a verified email: it becomes an "
                    "administrator once it has",
                    admin);
            }
            done(outcome->promoted);
        });
    if (!queued) {
        later(done, std::optional<std::size_t>{});
    }
}

} // namespace rtype::server
