#include "Accounts/AccountService.hpp"

#include "Accounts/AccountRules.hpp"
#include "Accounts/MailMessages.hpp"
#include "Common/Secret.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/Crypto.hpp"

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
    };
}

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
        if (_discord == nullptr) {
            // Not offered by this master: the payload is not read
            later(done, ErrorCode::AccountsDisabled);
            return;
        }
        break;
    default:
        break;
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
    later(done, rtype::lobby::DiscordAuthorizeMessage{.applicationId = _discord->applicationId()});
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
            [id, discordId = user.id](rtype::database::Database &database) {
                return linkDiscord(database, id, discordId, SystemClock::now());
            },
            done,
            [done, connection, id](DiscordLink link) {
                switch (link) {
                case DiscordLink::Linked:
                case DiscordLink::AlreadyLinked:
                    logger().info("Account {} {} its Discord identity", id,
                                  link == DiscordLink::Linked ? "linked" : "confirmed");
                    connection.discord->confirmedAt = SteadyClock::now();
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
        [discordId = user.id](rtype::database::Database &database)
            -> std::pair<std::optional<Account>, std::optional<SessionToken>> {
            std::optional<Account> account = findAccountByDiscord(database, discordId);
            if (!account || account->banned()) {
                return {std::move(account), std::nullopt};
            }
            return {std::move(account),
                    createSession(database, account->id, SessionClient::Game, SystemClock::now())};
        },
        done,
        [done, connection,
         user](const std::pair<std::optional<Account>, std::optional<SessionToken>> &found) {
            const auto &[account, token] = found;
            if (!account) {
                // A new user: CHOOSE_USERNAME makes its account
                connection.discord->newUser = user;
                connection.discord->newUserAt = SteadyClock::now();
                done(AccountDone{.closes = false});
                return;
            }
            if (!token) {
                logger().info("Account {} refused: it is banned", account->id);
                done(ErrorCode::Banned);
                return;
            }
            logger().info("Account {} signed in with Discord", account->id);
            done(SignedInAccount{.account = accountInfo(*account), .token = *token});
        });
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
        [username = request->username,
         discordId = progress.newUser->id](rtype::database::Database &database) {
            const auto now = SystemClock::now();
            Created created{createDiscordAccount(database, username, discordId, now), std::nullopt};
            if (const auto *account = std::get_if<Account>(&created.first)) {
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
            connection.discord->newUser.reset();
            logger().info("Account {} created with Discord, and signed in", account.id);
            done(SignedInAccount{.account = accountInfo(account), .token = *created.second});
        });
}

} // namespace rtype::server
