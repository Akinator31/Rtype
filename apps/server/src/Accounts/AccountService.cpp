#include "Accounts/AccountService.hpp"

#include "Accounts/AccountRules.hpp"
#include "Accounts/MailMessages.hpp"
#include "Common/Secret.hpp"
#include "Master/AdminStore.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/Crypto.hpp"
#include "rtype/network/Text.hpp"

#include <algorithm>
#include <format>
#include <type_traits>
#include <utility>

namespace rtype::server
{

namespace
{

using rtype::lobby::ErrorCode;
using SystemClock = std::chrono::system_clock;
namespace message_type = rtype::lobby::message_type;

const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("accounts");
    return instance;
}

/// A login is an email when it has an `@`, which no username has.
bool isEmailLogin(std::string_view login)
{
    return login.find('@') != std::string_view::npos;
}

std::optional<Account> findByLogin(rtype::database::Database &database, std::string_view login)
{
    return isEmailLogin(login) ? findAccountByEmail(database, login) : findAccountByUsername(database, login);
}

/// What two logins of the same account written alike share.
std::string loginKey(std::string_view login)
{
    if (isEmailLogin(login)) {
        const std::optional<std::string> email = normalizeEmail(login);
        return "email:" + emailKey(email.value_or(std::string(login)));
    }
    return "username:" + usernameKey(login);
}

rtype::lobby::AccountInfo accountInfo(const Account &account)
{
    return {
        .id = static_cast<std::uint64_t>(account.id),
        .username = account.username,
        .role = account.role == AccountRole::Admin ? rtype::lobby::AccountRole::Admin
                                                   : rtype::lobby::AccountRole::Player,
        .email = account.email.value_or(""),
        .emailVerified = account.verified(),
        .hasPassword = account.passwordHash.has_value(),
        .createdAt = static_cast<std::uint64_t>(std::max(unixMilliseconds(account.createdAt), 0LL)),
    };
}

/// A text of Discord as PROFILE carries it: whole, or empty if it does not fit.
std::string profileText(const std::string &text, std::size_t maxLength)
{
    return text.size() <= maxLength && rtype::network::isValidText(text) ? text : std::string();
}

/// The Discord profile as PROFILE carries it, its picture announced by its size.
rtype::lobby::ProfileMessage profileMessage(std::uint64_t accountId, const DiscordProfile &profile)
{
    rtype::lobby::ProfileMessage message{.accountId = accountId, .discord = true};
    message.discordUsername = profileText(profile.username, rtype::lobby::MaxDiscordUsernameLength);
    message.displayName = profileText(profile.displayName, rtype::lobby::MaxDisplayNameLength);
    const std::string hash = profileText(profile.avatarHash, rtype::lobby::MaxAvatarHashLength);
    if (!hash.empty() && !profile.avatar.empty() && profile.avatar.size() <= rtype::lobby::MaxAvatarSize) {
        message.avatarHash = hash;
        message.avatarSize = static_cast<std::uint32_t>(profile.avatar.size());
    }
    return message;
}

/// A part of a picture, from `offset`: NOT_FOUND without picture, or from beyond its end.
AccountReply avatarPart(std::uint64_t accountId, const std::optional<DiscordProfile> &profile,
                        std::uint32_t offset)
{
    const std::size_t size = profile ? profile->avatar.size() : 0;
    if (size == 0 || size > rtype::lobby::MaxAvatarSize || profile->avatarHash.empty() ||
        profile->avatarHash.size() > rtype::lobby::MaxAvatarHashLength || offset >= size) {
        return ErrorCode::NotFound;
    }
    const std::size_t part = std::min(rtype::lobby::MaxAvatarChunk, size - offset);
    const auto *first = reinterpret_cast<const std::byte *>(profile->avatar.data()) + offset;
    return rtype::lobby::AvatarMessage{.accountId = accountId,
                                       .avatarHash = profile->avatarHash,
                                       .total = static_cast<std::uint32_t>(size),
                                       .offset = offset,
                                       .bytes = std::vector<std::byte>(first, first + part)};
}

/// What GET_PROFILE and GET_AVATAR read: whether the account exists, and its Discord profile with its
/// picture.
struct ProfileRead {
    bool found = false;
    std::optional<DiscordProfile> profile;
};

ErrorCode codeError(CodeCheck check)
{
    switch (check) {
    case CodeCheck::Expired:
        return ErrorCode::CodeExpired;
    case CodeCheck::TooManyAttempts:
    case CodeCheck::DailyLimit:
    case CodeCheck::DailyLimitReached:
        return ErrorCode::TooManyAttempts;
    case CodeCheck::Accepted:
    case CodeCheck::Wrong:
    case CodeCheck::NoCode:
        break;
    }
    return ErrorCode::InvalidCode;
}

/// What a request that checks a code finds in the database.
struct CodeOutcome {
    CodeCheck check = CodeCheck::NoCode;
    std::optional<Account> account;
    /// The session opened, when the code is accepted and the account not banned.
    std::optional<SessionToken> token;
    /// RESET_PASSWORD: the new password is the username of the account.
    bool weak = false;
};

/// A code made for an account, to send to its email.
struct CodeToSend {
    std::string username;
    std::string email;
    std::string code;
};

/// What REGISTER finds in the database.
struct Registered {
    enum class Outcome {
        Created,
        UsernameTaken,
        /// The email has an account, whose username is given: its holder is told.
        EmailTaken,
    };
    Outcome outcome = Outcome::Created;
    std::string username;
    std::string code;
};

/// Keeps the tokens of a Discord user for the lobby, which sends them in DISCORD_TOKEN before the reply.
void keepToken(DiscordProgress &progress, const DiscordUser &user)
{
    if (user.tokens) {
        progress.token = rtype::lobby::DiscordTokenMessage{.accessToken = user.tokens->accessToken,
                                                           .refreshToken = user.tokens->refreshToken,
                                                           .expiresIn = user.tokens->expiresIn};
    }
}

} // namespace

AccountService::AccountService(rtype::network::EventLoop &loop, rtype::database::DatabaseThread &database,
                               PasswordHasher &hasher, Mailer &mailer, AccountServiceSettings settings,
                               DiscordClient *discord)
    : _loop(loop), _database(database), _hasher(hasher), _mailer(mailer), _discord(discord),
      _settings(settings),
      _dummyHash(hashPassword(formatSecret(rtype::network::randomBytes<SecretSize>()), hasher.parameters())),
      _purge(loop)
{
    purge();
    _purge.startRepeating(_settings.limits.purgeInterval, [this] {
        purge();
        forgetOld();
    });
}

AccountService::~AccountService()
{
    _purge.cancel();
}

rtype::lobby::ServerFeatures AccountService::features() const
{
    return {.guests = _settings.guests, .passwordAccounts = true, .discord = _discord != nullptr};
}

void AccountService::later(const Done &done, AccountReply reply)
{
    _loop.post([alive = std::weak_ptr<bool>(_alive), done, reply = std::move(reply)] {
        if (!alive.expired()) {
            done(reply);
        }
    });
}

template <typename Task, typename Then>
void AccountService::onDatabase(std::string description, Task task, const Done &done, Then then)
{
    using Result = std::invoke_result_t<Task &, rtype::database::Database &>;
    const bool queued = _database.run(std::move(description), std::move(task),
                                      [alive = std::weak_ptr<bool>(_alive), done,
                                       then = std::move(then)](std::optional<Result> result) mutable {
                                          if (alive.expired()) {
                                              return;
                                          }
                                          if (!result) {
                                              done(ErrorCode::Busy);
                                              return;
                                          }
                                          then(std::move(*result));
                                      });
    if (!queued) {
        logger().warning("The database thread is full: a request of the accounts gets BUSY");
        later(done, ErrorCode::Busy);
    }
}

void AccountService::hash(std::string password, const Done &done, std::function<void(std::string)> then)
{
    const bool queued =
        _hasher.hash(std::move(password), [alive = std::weak_ptr<bool>(_alive), done,
                                           then = std::move(then)](std::optional<std::string> hash) {
            if (alive.expired()) {
                return;
            }
            if (!hash) {
                done(ErrorCode::Busy);
                return;
            }
            then(std::move(*hash));
        });
    if (!queued) {
        later(done, ErrorCode::Busy);
    }
}

void AccountService::verify(std::string hash, std::string password, const Done &done,
                            std::function<void(bool)> then)
{
    const bool queued =
        _hasher.verify(std::move(hash), std::move(password),
                       [alive = std::weak_ptr<bool>(_alive), then = std::move(then)](bool matches) {
                           if (!alive.expired()) {
                               then(matches);
                           }
                       });
    if (!queued) {
        later(done, ErrorCode::Busy);
    }
}

bool AccountService::mail(const std::string &to, const MailContent &content)
{
    switch (_mailer.send(Mail{.to = to, .content = content})) {
    case MailQueued::Yes:
        return true;
    case MailQueued::Busy:
        return false;
    case MailQueued::AddressLimit:
        // Answered as sent: the answer must not tell whether the address has an account
        logger().info("Mail \"{}\" not sent: its address got its most mails of the hour", content.subject);
        return true;
    case MailQueued::InvalidAddress:
        logger().warning("Mail \"{}\" not sent: its address is not one", content.subject);
        return true;
    }
    return true;
}

bool AccountService::allowAddress(const rtype::network::Address &address)
{
    const auto now = SteadyClock::now();
    std::deque<SteadyClock::time_point> &times = _requests[address.withPort(0)];
    while (!times.empty() && times.front() <= now - _settings.limits.addressWindow) {
        times.pop_front();
    }
    if (times.size() >= _settings.limits.requestsPerAddress) {
        return false;
    }
    times.push_back(now);
    return true;
}

bool AccountService::paused(long long accountId) const
{
    const auto found = _strikes.find(accountId);
    return found != _strikes.end() && found->second.count >= _settings.limits.wrongPasswords &&
           SteadyClock::now() < found->second.pausedUntil;
}

void AccountService::wrongPassword(long long accountId)
{
    Strikes &strikes = _strikes[accountId];
    if (strikes.count >= _settings.limits.wrongPasswords) {
        // The pause is over: counted again from the start
        strikes.count = 0;
    }
    if (++strikes.count >= _settings.limits.wrongPasswords) {
        strikes.pausedUntil = SteadyClock::now() + _settings.limits.wrongPasswordPause;
        logger().warning("Account {} paused after {} wrong passwords", accountId, strikes.count);
    }
}

void AccountService::purge()
{
    (void)_database.run(
        "purge the accounts not verified",
        [](rtype::database::Database &database) {
            return purgeUnverifiedAccounts(database, SystemClock::now());
        },
        [](std::optional<std::size_t> purged) {
            if (purged && *purged != 0) {
                logger().info("{} accounts not verified within 24 hours deleted", *purged);
            }
        });
}

void AccountService::forgetOld()
{
    const auto now = SteadyClock::now();
    std::erase_if(_requests, [&](const auto &entry) {
        return entry.second.empty() || entry.second.back() <= now - _settings.limits.addressWindow;
    });
    std::erase_if(_strikes, [&](const auto &entry) {
        return entry.second.count >= _settings.limits.wrongPasswords && entry.second.pausedUntil <= now;
    });
    std::erase_if(_resent,
                  [&](const auto &entry) { return entry.second <= now - _settings.limits.resendInterval; });
}

void AccountService::resume(const SessionToken &token,
                            std::function<void(std::variant<rtype::lobby::AccountInfo, ErrorCode>)> done)
{
    const bool queued = _database.run(
        "resume a session",
        [token](rtype::database::Database &database) {
            return resumeSession(database, token, SessionClient::Game, SystemClock::now());
        },
        [alive = std::weak_ptr<bool>(_alive), done](std::optional<std::optional<Account>> account) {
            if (alive.expired()) {
                return;
            }
            if (!account) {
                done(ErrorCode::Busy);
            } else if (!*account) {
                done(ErrorCode::SessionExpired);
            } else {
                logger().info("Account {} resumed its session", (*account)->id);
                done(accountInfo(**account));
            }
        });
    if (!queued) {
        _loop.post([alive = std::weak_ptr<bool>(_alive), done] {
            if (!alive.expired()) {
                done(ErrorCode::Busy);
            }
        });
    }
}

template <typename Task>
void AccountService::onProgress(std::string description, Task task, ProgressDone done)
{
    const bool queued =
        _database.run(std::move(description), std::move(task),
                      [alive = std::weak_ptr<bool>(_alive), done](std::optional<AccountProgress> progress) {
                          if (!alive.expired()) {
                              done(progress);
                          }
                      });
    if (!queued) {
        logger().warning("The database thread is full: a progression is not read nor written");
        _loop.post([alive = std::weak_ptr<bool>(_alive), done] {
            if (!alive.expired()) {
                done(std::nullopt);
            }
        });
    }
}

void AccountService::progress(long long accountId, ProgressDone done)
{
    onProgress(
        "read the progression of an account",
        [accountId](rtype::database::Database &database) { return readProgress(database, accountId); },
        std::move(done));
}

void AccountService::recordGame(long long accountId, GameResult game, ProgressDone done)
{
    onProgress(
        "add a game to the progression of an account",
        [accountId, game](rtype::database::Database &database) { return addGame(database, accountId, game); },
        std::move(done));
}

void AccountService::checkGuestName(const std::string &name,
                                    std::function<void(std::optional<ErrorCode>)> done)
{
    const auto answer = [alive = std::weak_ptr<bool>(_alive), done](std::optional<ErrorCode> error) {
        if (!alive.expired()) {
            done(error);
        }
    };
    if (!isValidUsername(name)) {
        // No account can have it
        _loop.post([answer] { answer(std::nullopt); });
        return;
    }
    const bool queued = _database.run(
        "check the name of a guest",
        [name](rtype::database::Database &database) {
            return findAccountByUsername(database, name).has_value();
        },
        [answer](std::optional<bool> taken) {
            if (!taken) {
                answer(ErrorCode::Busy);
            } else {
                answer(*taken ? std::optional(ErrorCode::NameReserved) : std::nullopt);
            }
        });
    if (!queued) {
        _loop.post([answer] { answer(ErrorCode::Busy); });
    }
}

void AccountService::handle(std::uint8_t type, std::span<const std::byte> payload,
                            const AccountConnection &connection, Done done)
{
    switch (type) {
    case message_type::DiscordSignIn:
    case message_type::DiscordConfirm:
    case message_type::ChooseUsername:
    case message_type::DiscordRefresh:
        if (_discord == nullptr) {
            // Not offered by this master: the payload is not read
            later(done, ErrorCode::AccountsDisabled);
            return;
        }
        break;
    default:
        break;
    }
    // The profiles only read: they do not use up the requests of an address
    if (type == message_type::GetProfile) {
        getProfile(payload, std::move(done));
        return;
    }
    if (type == message_type::GetAvatar) {
        getAvatar(connection, payload, std::move(done));
        return;
    }
    if (!allowAddress(connection.address)) {
        logger().info("{}: too many requests of the accounts", connection.address.ip());
        later(done, ErrorCode::TooManyAttempts);
        return;
    }
    switch (type) {
    case message_type::Register:
        registerAccount(connection, payload, std::move(done));
        return;
    case message_type::VerifyEmail:
        verifyEmail(connection, payload, std::move(done));
        return;
    case message_type::ResendCode:
        resendCode(connection, payload, std::move(done));
        return;
    case message_type::SignIn:
        signIn(connection, payload, std::move(done));
        return;
    case message_type::SignOut:
        signOut(connection, payload, std::move(done));
        return;
    case message_type::ForgotPassword:
        forgotPassword(connection, payload, std::move(done));
        return;
    case message_type::ResetPassword:
        resetPassword(connection, payload, std::move(done));
        return;
    case message_type::DeleteAccount:
        deleteAccount(connection, payload, std::move(done));
        return;
    case message_type::DiscordSignIn:
        discordSignIn(connection, payload, std::move(done));
        return;
    case message_type::DiscordConfirm:
        discordConfirm(connection, payload, std::move(done));
        return;
    case message_type::ChooseUsername:
        chooseUsername(connection, payload, std::move(done));
        return;
    case message_type::DiscordRefresh:
        discordRefresh(connection, payload, std::move(done));
        return;
    default:
        later(done, ErrorCode::UnknownMessage);
        return;
    }
}

void AccountService::registerAccount(const AccountConnection &connection, std::span<const std::byte> payload,
                                     Done done)
{
    std::optional<rtype::lobby::RegisterMessage> request = rtype::lobby::decodeRegister(payload);
    if (!request) {
        later(done, ErrorCode::Malformed);
        return;
    }
    if (connection.signedIn) {
        later(done, ErrorCode::AlreadySignedIn);
        return;
    }
    if (!isValidUsername(request->username)) {
        later(done, ErrorCode::InvalidName);
        return;
    }
    const std::optional<std::string> email = normalizeEmail(request->email);
    if (!email) {
        later(done, ErrorCode::InvalidEmail);
        return;
    }
    if (checkPassword(request->password, request->username, *email) != PasswordProblem::None) {
        later(done, ErrorCode::WeakPassword);
        return;
    }
    // Hashed whether the email has an account or not: the answer takes as long
    const MailLanguage language = mailLanguage(connection.locale);
    hash(std::move(request->password), done,
         [this, done, language, username = request->username, email = *email](std::string passwordHash) {
             onDatabase(
                 "register an account",
                 [username, email, passwordHash](rtype::database::Database &database) {
                     const auto now = SystemClock::now();
                     const auto created = createAccount(database,
                                                        {.username = username,
                                                         .email = email,
                                                         .passwordHash = passwordHash,
                                                         .role = AccountRole::Player},
                                                        now);
                     if (const auto *account = std::get_if<Account>(&created)) {
                         return Registered{.outcome = Registered::Outcome::Created,
                                           .username = account->username,
                                           .code =
                                               createCode(database, account->id, CodePurpose::Verify, now)};
                     }
                     if (std::get<AccountConflict>(created) == AccountConflict::UsernameTaken) {
                         return Registered{
                             .outcome = Registered::Outcome::UsernameTaken, .username = {}, .code = {}};
                     }
                     const std::optional<Account> existing = findAccountByEmail(database, email);
                     return Registered{.outcome = Registered::Outcome::EmailTaken,
                                       .username = existing ? existing->username : username,
                                       .code = {}};
                 },
                 done,
                 [this, done, language, email](const Registered &registered) {
                     switch (registered.outcome) {
                     case Registered::Outcome::UsernameTaken:
                         done(ErrorCode::UsernameTaken);
                         return;
                     case Registered::Outcome::Created:
                         logger().info("Account {} created, its email not verified", registered.username);
                         if (!mail(email, verificationMail(language, registered.username, registered.code,
                                                           CodeLifetime))) {
                             done(ErrorCode::MailUnavailable);
                             return;
                         }
                         break;
                     case Registered::Outcome::EmailTaken:
                         if (!mail(email, existingAccountMail(language, registered.username))) {
                             done(ErrorCode::MailUnavailable);
                             return;
                         }
                         break;
                     }
                     done(CodeSent{});
                 });
         });
}

void AccountService::verifyEmail(const AccountConnection &connection, std::span<const std::byte> payload,
                                 Done done)
{
    const std::optional<rtype::lobby::VerifyEmailMessage> request = rtype::lobby::decodeVerifyEmail(payload);
    if (!request) {
        later(done, ErrorCode::Malformed);
        return;
    }
    if (connection.signedIn) {
        later(done, ErrorCode::AlreadySignedIn);
        return;
    }
    const MailLanguage language = mailLanguage(connection.locale);
    onDatabase(
        "verify an email",
        [login = request->login, code = request->code](rtype::database::Database &database) {
            CodeOutcome outcome;
            outcome.account = findByLogin(database, login);
            if (!outcome.account) {
                return outcome;
            }
            const auto now = SystemClock::now();
            outcome.check = checkCode(database, outcome.account->id, CodePurpose::Verify, code, now);
            if (outcome.check == CodeCheck::Accepted) {
                markEmailVerified(database, outcome.account->id, now);
                if (!outcome.account->banned()) {
                    outcome.token = createSession(database, outcome.account->id, SessionClient::Game, now);
                }
            }
            return outcome;
        },
        done,
        [this, done, language](const CodeOutcome &outcome) {
            if (outcome.check == CodeCheck::DailyLimitReached && outcome.account && outcome.account->email) {
                (void)mail(*outcome.account->email, tooManyCodesMail(language, outcome.account->username));
            }
            if (outcome.check != CodeCheck::Accepted) {
                done(codeError(outcome.check));
                return;
            }
            if (!outcome.token) {
                done(ErrorCode::Banned);
                return;
            }
            logger().info("Account {} verified its email, and signed in", outcome.account->id);
            done(SignedInAccount{.account = accountInfo(*outcome.account), .token = *outcome.token});
        });
}

void AccountService::resendCode(const AccountConnection &connection, std::span<const std::byte> payload,
                                Done done)
{
    const std::optional<rtype::lobby::ResendCodeMessage> request = rtype::lobby::decodeResendCode(payload);
    if (!request) {
        later(done, ErrorCode::Malformed);
        return;
    }
    if (connection.signedIn) {
        later(done, ErrorCode::AlreadySignedIn);
        return;
    }
    // Counted by login, before the database: the answer is the same whether it has an account or not
    const auto now = SteadyClock::now();
    const std::string key = loginKey(request->login);
    if (const auto last = _resent.find(key);
        last != _resent.end() && now - last->second < _settings.limits.resendInterval) {
        later(done, ErrorCode::TooManyAttempts);
        return;
    }
    _resent[key] = now;
    const MailLanguage language = mailLanguage(connection.locale);
    onDatabase(
        "send a code again",
        [login = request->login](rtype::database::Database &database) -> std::optional<CodeToSend> {
            const std::optional<Account> account = findByLogin(database, login);
            if (!account || account->verified() || !account->email) {
                return std::nullopt;
            }
            return CodeToSend{.username = account->username,
                              .email = *account->email,
                              .code =
                                  createCode(database, account->id, CodePurpose::Verify, SystemClock::now())};
        },
        done,
        [this, done, language](const std::optional<CodeToSend> &code) {
            if (code &&
                !mail(code->email, verificationMail(language, code->username, code->code, CodeLifetime))) {
                done(ErrorCode::MailUnavailable);
                return;
            }
            done(CodeSent{});
        });
}

void AccountService::signIn(const AccountConnection &connection, std::span<const std::byte> payload,
                            Done done)
{
    std::optional<rtype::lobby::SignInMessage> request = rtype::lobby::decodeSignIn(payload);
    if (!request) {
        later(done, ErrorCode::Malformed);
        return;
    }
    if (connection.signedIn) {
        later(done, ErrorCode::AlreadySignedIn);
        return;
    }
    const MailLanguage language = mailLanguage(connection.locale);
    onDatabase(
        "find an account to sign in",
        [login = request->login](rtype::database::Database &database) {
            return findByLogin(database, login);
        },
        done,
        [this, done, language,
         password = std::move(request->password)](const std::optional<Account> &account) {
            if (account && paused(account->id)) {
                done(ErrorCode::TooManyAttempts);
                return;
            }
            // Without an account, a password is checked all the same, against nothing: the answer takes as
            // long
            const bool hasPassword = account && account->passwordHash;
            verify(hasPassword ? *account->passwordHash : _dummyHash, password, done,
                   [this, done, language, account, hasPassword](bool matches) {
                       if (!matches || !hasPassword) {
                           if (account) {
                               wrongPassword(account->id);
                           }
                           done(ErrorCode::InvalidCredentials);
                           return;
                       }
                       _strikes.erase(account->id);
                       if (account->banned()) {
                           logger().info("Account {} refused: it is banned", account->id);
                           done(ErrorCode::Banned);
                           return;
                       }
                       const long long id = account->id;
                       if (!account->verified()) {
                           // Only now, after the right password: a new code to verify it
                           onDatabase(
                               "send a code to an account not verified",
                               [id](rtype::database::Database &database) {
                                   return createCode(database, id, CodePurpose::Verify, SystemClock::now());
                               },
                               done,
                               [this, done, language, account](const std::string &code) {
                                   if (!mail(*account->email, verificationMail(language, account->username,
                                                                               code, CodeLifetime))) {
                                       done(ErrorCode::MailUnavailable);
                                       return;
                                   }
                                   done(ErrorCode::EmailNotVerified);
                               });
                           return;
                       }
                       onDatabase(
                           "open a session",
                           [id](rtype::database::Database &database) {
                               return createSession(database, id, SessionClient::Game, SystemClock::now());
                           },
                           done,
                           [done, account](const SessionToken &token) {
                               logger().info("Account {} signed in", account->id);
                               done(SignedInAccount{.account = accountInfo(*account), .token = token});
                           });
                   });
        });
}

void AccountService::signOut(const AccountConnection &connection, std::span<const std::byte> payload,
                             Done done)
{
    if (!rtype::lobby::decodeEmpty(payload)) {
        later(done, ErrorCode::Malformed);
        return;
    }
    if (!connection.signedIn) {
        later(done, ErrorCode::NotSignedIn);
        return;
    }
    const std::uint64_t id = connection.signedIn->account.id;
    onDatabase(
        "end a session",
        [token = connection.signedIn->token](rtype::database::Database &database) {
            return endSession(database, token);
        },
        done,
        [done, id](bool) {
            logger().info("Account {} signed out", id);
            done(AccountDone{.closes = true});
        });
}

void AccountService::forgotPassword(const AccountConnection &connection, std::span<const std::byte> payload,
                                    Done done)
{
    const std::optional<rtype::lobby::ForgotPasswordMessage> request =
        rtype::lobby::decodeForgotPassword(payload);
    if (!request) {
        later(done, ErrorCode::Malformed);
        return;
    }
    if (connection.signedIn) {
        later(done, ErrorCode::AlreadySignedIn);
        return;
    }
    const std::optional<std::string> email = normalizeEmail(request->email);
    if (!email) {
        later(done, ErrorCode::InvalidEmail);
        return;
    }
    const MailLanguage language = mailLanguage(connection.locale);
    onDatabase(
        "send a code for a new password",
        [email = *email](rtype::database::Database &database) -> std::optional<CodeToSend> {
            const std::optional<Account> account = findAccountByEmail(database, email);
            if (!account || !account->email) {
                return std::nullopt;
            }
            return CodeToSend{.username = account->username,
                              .email = *account->email,
                              .code =
                                  createCode(database, account->id, CodePurpose::Reset, SystemClock::now())};
        },
        done,
        [this, done, language](const std::optional<CodeToSend> &code) {
            if (code && !mail(code->email, resetMail(language, code->username, code->code, CodeLifetime))) {
                done(ErrorCode::MailUnavailable);
                return;
            }
            done(CodeSent{});
        });
}

void AccountService::resetPassword(const AccountConnection &connection, std::span<const std::byte> payload,
                                   Done done)
{
    std::optional<rtype::lobby::ResetPasswordMessage> request = rtype::lobby::decodeResetPassword(payload);
    if (!request) {
        later(done, ErrorCode::Malformed);
        return;
    }
    if (connection.signedIn) {
        later(done, ErrorCode::AlreadySignedIn);
        return;
    }
    const std::optional<std::string> email = normalizeEmail(request->email);
    if (!email) {
        later(done, ErrorCode::InvalidEmail);
        return;
    }
    // The username is checked once the account is found
    if (checkPassword(request->password, "", *email) != PasswordProblem::None) {
        later(done, ErrorCode::WeakPassword);
        return;
    }
    const MailLanguage language = mailLanguage(connection.locale);
    std::string password = request->password;
    hash(std::move(request->password), done,
         [this, done, language, email = *email, code = request->code,
          password = std::move(password)](std::string passwordHash) {
             onDatabase(
                 "set a new password",
                 [email, code, password, passwordHash](rtype::database::Database &database) {
                     CodeOutcome outcome;
                     outcome.account = findAccountByEmail(database, email);
                     if (!outcome.account) {
                         return outcome;
                     }
                     if (checkPassword(password, outcome.account->username, email) != PasswordProblem::None) {
                         outcome.weak = true;
                         return outcome;
                     }
                     const auto now = SystemClock::now();
                     outcome.check = checkCode(database, outcome.account->id, CodePurpose::Reset, code, now);
                     if (outcome.check != CodeCheck::Accepted) {
                         return outcome;
                     }
                     // Every session ends: whoever knew the old password is signed out
                     setPassword(database, outcome.account->id, passwordHash);
                     if (!outcome.account->verified()) {
                         markEmailVerified(database, outcome.account->id, now);
                     }
                     if (!outcome.account->banned()) {
                         outcome.token =
                             createSession(database, outcome.account->id, SessionClient::Game, now);
                     }
                     return outcome;
                 },
                 done,
                 [this, done, language](const CodeOutcome &outcome) {
                     if (outcome.weak) {
                         done(ErrorCode::WeakPassword);
                         return;
                     }
                     if (outcome.check == CodeCheck::DailyLimitReached && outcome.account->email) {
                         (void)mail(*outcome.account->email,
                                    tooManyCodesMail(language, outcome.account->username));
                     }
                     if (outcome.check != CodeCheck::Accepted) {
                         done(codeError(outcome.check));
                         return;
                     }
                     if (!outcome.token) {
                         done(ErrorCode::Banned);
                         return;
                     }
                     _strikes.erase(outcome.account->id);
                     logger().info("Account {} set a new password, and signed in", outcome.account->id);
                     done(SignedInAccount{.account = accountInfo(*outcome.account), .token = *outcome.token});
                 });
         });
}

void AccountService::deleteAccount(const AccountConnection &connection, std::span<const std::byte> payload,
                                   Done done)
{
    std::optional<rtype::lobby::DeleteAccountMessage> request = rtype::lobby::decodeDeleteAccount(payload);
    if (!request) {
        later(done, ErrorCode::Malformed);
        return;
    }
    if (!connection.signedIn) {
        later(done, ErrorCode::NotSignedIn);
        return;
    }
    const auto id = static_cast<long long>(connection.signedIn->account.id);
    if (paused(id)) {
        later(done, ErrorCode::TooManyAttempts);
        return;
    }
    const std::function<void()> remove = [this, done, id] {
        onDatabase(
            "delete an account",
            [id](rtype::database::Database &database) { return rtype::server::deleteAccount(database, id); },
            done,
            [this, done, id](bool) {
                _strikes.erase(id);
                logger().info("Account {} deleted by its owner", id);
                done(AccountDone{.closes = true});
            });
    };
    // Confirmed with Discord on this connection, not too long ago: what an account without password gives
    const std::optional<DiscordProgress::Time> confirmed = connection.discord->confirmedAt;
    const bool discordConfirmed =
        confirmed && SteadyClock::now() - *confirmed <= _settings.limits.deletionConfirmation;
    onDatabase(
        "find an account to delete",
        [id](rtype::database::Database &database) { return findAccount(database, id); }, done,
        [this, done, id, remove, discordConfirmed,
         password = std::move(request->password)](const std::optional<Account> &account) {
            if (!account) {
                done(ErrorCode::NotSignedIn);
                return;
            }
            if (!account->passwordHash) {
                if (!password.empty()) {
                    done(ErrorCode::InvalidCredentials);
                } else if (!discordConfirmed) {
                    done(ErrorCode::DiscordNotConfirmed);
                } else {
                    remove();
                }
                return;
            }
            if (password.empty()) {
                done(ErrorCode::InvalidCredentials);
                return;
            }
            verify(*account->passwordHash, password, done, [this, done, id, remove](bool matches) {
                if (!matches) {
                    wrongPassword(id);
                    done(ErrorCode::InvalidCredentials);
                    return;
                }
                remove();
            });
        });
}

void AccountService::discordSignIn(const AccountConnection &connection, std::span<const std::byte> payload,
                                   Done done)
{
    if (!rtype::lobby::decodeEmpty(payload)) {
        later(done, ErrorCode::Malformed);
        return;
    }
    // A new attempt: the codes refused before are forgotten, and with them a new user not given a name
    DiscordProgress &progress = *connection.discord;
    progress.startedAt = SteadyClock::now();
    progress.refused = 0;
    progress.newUser.reset();
    // The social features of the Social SDK need the token of the player, which these scopes give
    later(done, rtype::lobby::DiscordAuthorizeMessage{
                    .applicationId = _discord->applicationId(),
                    .scopes = _discord->social() ? "identify openid sdk.social_layer" : "identify"});
}

void AccountService::discordConfirm(const AccountConnection &connection, std::span<const std::byte> payload,
                                    Done done)
{
    std::optional<rtype::lobby::DiscordConfirmMessage> request = rtype::lobby::decodeDiscordConfirm(payload);
    if (!request) {
        later(done, ErrorCode::Malformed);
        return;
    }
    DiscordProgress &progress = *connection.discord;
    if (!progress.startedAt) {
        later(done, ErrorCode::DiscordNotConfirmed);
        return;
    }
    if (SteadyClock::now() - *progress.startedAt > _settings.limits.discordLifetime) {
        progress.startedAt.reset();
        later(done, ErrorCode::CodeExpired);
        return;
    }
    if (progress.refused >= _settings.limits.discordAttempts) {
        later(done, ErrorCode::TooManyAttempts);
        return;
    }
    const bool queued = _discord->identify(
        DiscordAuthorization{.code = std::move(request->code),
                             .codeVerifier = std::move(request->codeVerifier),
                             .redirectUri = std::move(request->redirectUri)},
        [this, alive = std::weak_ptr<bool>(_alive), connection, done](const DiscordAnswer &answer) {
            if (alive.expired()) {
                return;
            }
            DiscordProgress &progress = *connection.discord;
            if (const auto *failure = std::get_if<DiscordFailure>(&answer)) {
                if (*failure == DiscordFailure::Refused) {
                    ++progress.refused;
                    done(ErrorCode::InvalidCode);
                } else {
                    done(ErrorCode::DiscordUnavailable);
                }
                return;
            }
            // Used: another DISCORD_CONFIRM needs another DISCORD_SIGN_IN
            progress.startedAt.reset();
            discordUser(connection, std::get<DiscordUser>(answer), done);
        });
    if (!queued) {
        later(done, ErrorCode::Busy);
    }
}

void AccountService::discordUser(const AccountConnection &connection, const DiscordUser &user,
                                 const Done &done)
{
    if (connection.signedIn) {
        // Signed in: the user becomes the identity of the account, or proves it is
        const auto id = static_cast<long long>(connection.signedIn->account.id);
        onDatabase(
            "link a Discord identity",
            [id, user](rtype::database::Database &database) {
                const auto now = SystemClock::now();
                const DiscordLink link = linkDiscord(database, id, user.id, now);
                if (link == DiscordLink::Linked || link == DiscordLink::AlreadyLinked) {
                    saveDiscordProfile(database, user.id, profileOf(user), now);
                }
                return link;
            },
            done,
            [done, connection, id, user](DiscordLink link) {
                switch (link) {
                case DiscordLink::Linked:
                case DiscordLink::AlreadyLinked:
                    logger().info("Account {} {} its Discord identity", id,
                                  link == DiscordLink::Linked ? "linked" : "confirmed");
                    connection.discord->confirmedAt = SteadyClock::now();
                    keepToken(*connection.discord, user);
                    done(AccountDone{.closes = false});
                    return;
                case DiscordLink::Taken:
                    done(ErrorCode::DiscordTaken);
                    return;
                case DiscordLink::NoAccount:
                    done(ErrorCode::NotSignedIn);
                    return;
                }
            });
        return;
    }
    onDatabase(
        "sign in with Discord",
        [user](rtype::database::Database &database)
            -> std::pair<std::optional<Account>, std::optional<SessionToken>> {
            std::optional<Account> account = findAccountByDiscord(database, user.id);
            if (!account || account->banned()) {
                return {std::move(account), std::nullopt};
            }
            // The profile as Discord tells it now: its name or its picture may have changed
            saveDiscordProfile(database, user.id, profileOf(user), SystemClock::now());
            return {std::move(account),
                    createSession(database, account->id, SessionClient::Game, SystemClock::now())};
        },
        done,
        [done, connection,
         user](const std::pair<std::optional<Account>, std::optional<SessionToken>> &found) {
            const auto &[account, token] = found;
            if (!account) {
                // A new user: CHOOSE_USERNAME makes its account. Its profile shows while it chooses
                connection.discord->newUser = user;
                connection.discord->newUserAt = SteadyClock::now();
                done(profileMessage(0, profileOf(user)));
                return;
            }
            if (!token) {
                logger().info("Account {} refused: it is banned", account->id);
                done(ErrorCode::Banned);
                return;
            }
            logger().info("Account {} signed in with Discord", account->id);
            keepToken(*connection.discord, user);
            done(SignedInAccount{.account = accountInfo(*account), .token = *token});
        });
}

void AccountService::discordRefresh(const AccountConnection &connection, std::span<const std::byte> payload,
                                    Done done)
{
    std::optional<rtype::lobby::DiscordRefreshMessage> request = rtype::lobby::decodeDiscordRefresh(payload);
    if (!request) {
        later(done, ErrorCode::Malformed);
        return;
    }
    if (!connection.signedIn) {
        later(done, ErrorCode::NotSignedIn);
        return;
    }
    if (!_discord->social()) {
        // This master gives no token: there is none to renew
        later(done, ErrorCode::DiscordTokenRefused);
        return;
    }
    DiscordProgress &progress = *connection.discord;
    if (progress.refreshes >= _settings.limits.discordAttempts) {
        later(done, ErrorCode::TooManyAttempts);
        return;
    }
    ++progress.refreshes;
    const auto accountId = static_cast<long long>(connection.signedIn->account.id);
    const bool queued =
        _discord->refresh(std::move(request->refreshToken), [this, alive = std::weak_ptr<bool>(_alive), done,
                                                             accountId](const DiscordAnswer &answer) {
            if (alive.expired()) {
                return;
            }
            if (const auto *failure = std::get_if<DiscordFailure>(&answer)) {
                done(*failure == DiscordFailure::Refused ? ErrorCode::DiscordTokenRefused
                                                         : ErrorCode::DiscordUnavailable);
                return;
            }
            const DiscordUser &user = std::get<DiscordUser>(answer);
            // Only the token of the identity of the account: another one is not this player's to renew here
            onDatabase(
                "check the Discord identity of a refresh",
                [accountId](rtype::database::Database &database) {
                    return discordIdentity(database, accountId);
                },
                done,
                [done, accountId, user](const std::optional<std::string> &identity) {
                    if (identity != user.id) {
                        logger().info("Account {} refreshed a Discord token not its own", accountId);
                        done(ErrorCode::DiscordTokenRefused);
                        return;
                    }
                    logger().info("Account {} renewed its Discord token", accountId);
                    done(rtype::lobby::DiscordTokenMessage{.accessToken = user.tokens->accessToken,
                                                           .refreshToken = user.tokens->refreshToken,
                                                           .expiresIn = user.tokens->expiresIn});
                });
        });
    if (!queued) {
        later(done, ErrorCode::Busy);
    }
}

void AccountService::chooseUsername(const AccountConnection &connection, std::span<const std::byte> payload,
                                    Done done)
{
    const std::optional<rtype::lobby::ChooseUsernameMessage> request =
        rtype::lobby::decodeChooseUsername(payload);
    if (!request) {
        later(done, ErrorCode::Malformed);
        return;
    }
    if (connection.signedIn) {
        later(done, ErrorCode::AlreadySignedIn);
        return;
    }
    const DiscordProgress &progress = *connection.discord;
    if (!progress.newUser || SteadyClock::now() - progress.newUserAt > _settings.limits.discordLifetime) {
        later(done, ErrorCode::DiscordNotConfirmed);
        return;
    }
    if (!isValidUsername(request->username)) {
        later(done, ErrorCode::InvalidName);
        return;
    }
    using Created = std::pair<std::variant<Account, AccountConflict>, std::optional<SessionToken>>;
    onDatabase(
        "create an account with Discord",
        [username = request->username, user = *progress.newUser](rtype::database::Database &database) {
            const auto now = SystemClock::now();
            Created created{createDiscordAccount(database, username, user.id, now), std::nullopt};
            if (const auto *account = std::get_if<Account>(&created.first)) {
                saveDiscordProfile(database, user.id, profileOf(user), now);
                created.second = createSession(database, account->id, SessionClient::Game, now);
            }
            return created;
        },
        done,
        [done, connection](const Created &created) {
            if (const auto *conflict = std::get_if<AccountConflict>(&created.first)) {
                // EmailTaken: the identity got an account meanwhile, on another connection
                done(*conflict == AccountConflict::UsernameTaken ? ErrorCode::UsernameTaken
                                                                 : ErrorCode::DiscordTaken);
                return;
            }
            const Account &account = std::get<Account>(created.first);
            // Its account made, the new user gets its token
            if (connection.discord->newUser) {
                keepToken(*connection.discord, *connection.discord->newUser);
            }
            connection.discord->newUser.reset();
            logger().info("Account {} created with Discord, and signed in", account.id);
            done(SignedInAccount{.account = accountInfo(account), .token = *created.second});
        });
}

void AccountService::getProfile(std::span<const std::byte> payload, Done done)
{
    const std::optional<rtype::lobby::GetProfileMessage> request = rtype::lobby::decodeGetProfile(payload);
    if (!request) {
        later(done, ErrorCode::Malformed);
        return;
    }
    const auto id = static_cast<long long>(request->accountId);
    onDatabase(
        "read the profile of an account",
        [id](rtype::database::Database &database) {
            return ProfileRead{.found = findAccount(database, id).has_value(),
                               .profile = findDiscordProfile(database, id)};
        },
        done,
        [done, accountId = request->accountId](const ProfileRead &read) {
            if (!read.found) {
                done(ErrorCode::NotFound);
                return;
            }
            done(read.profile ? profileMessage(accountId, *read.profile)
                              : rtype::lobby::ProfileMessage{.accountId = accountId});
        });
}

void AccountService::getAvatar(const AccountConnection &connection, std::span<const std::byte> payload,
                               Done done)
{
    const std::optional<rtype::lobby::GetAvatarMessage> request = rtype::lobby::decodeGetAvatar(payload);
    if (!request) {
        later(done, ErrorCode::Malformed);
        return;
    }
    if (request->accountId == 0) {
        // The new Discord user of this connection: its picture is in memory, until it chooses its username
        const DiscordProgress &progress = *connection.discord;
        const bool current =
            progress.newUser && SteadyClock::now() - progress.newUserAt <= _settings.limits.discordLifetime;
        later(done, avatarPart(0, current ? std::optional(profileOf(*progress.newUser)) : std::nullopt,
                               request->offset));
        return;
    }
    const auto id = static_cast<long long>(request->accountId);
    onDatabase(
        "read the picture of an account",
        [id](rtype::database::Database &database) { return findDiscordProfile(database, id); }, done,
        [done, request = *request](const std::optional<DiscordProfile> &profile) {
            done(avatarPart(request.accountId, profile, request.offset));
        });
}

} // namespace rtype::server
