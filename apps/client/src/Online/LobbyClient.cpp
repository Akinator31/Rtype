#include "Online/LobbyClient.hpp"

#include "rtype/log/Logger.hpp"
#include "rtype/network/Errors.hpp"
#include "rtype/network/TcpConnection.hpp"
#include "rtype/network/Text.hpp"
#include "rtype/network/TlsConnection.hpp"

#include <format>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace rtype::client
{

namespace
{

namespace message_type = lobby::message_type;
using lobby::ErrorCode;

const log::Logger &logger()
{
    static const log::Logger instance("lobby");
    return instance;
}

/// What a closed connection means for the player.
std::string reasonOf(std::error_code reason)
{
    if (!reason) {
        return "The server closed the connection";
    }
    if (reason == network::NetworkError::UntrustedServer) {
        return "This is not the expected server: its key is not trusted";
    }
    return std::format("Connection lost: {}", reason.message());
}

/// Whether a field of the accounts fits its place in a request: 1 to `maxLength` bytes of text (0 to
/// `maxLength` if it may be empty).
bool fits(std::string_view text, std::size_t maxLength, bool allowEmpty = false)
{
    return (allowEmpty || !text.empty()) && text.size() <= maxLength && network::isValidText(text);
}

/// What the master gives a request answered SIGNED_IN or an error.
LobbyClient::SignInResult signInResult(std::uint8_t type, std::span<const std::byte> payload)
{
    if (type == message_type::SignedIn) {
        if (const auto signedIn = lobby::decodeSignedIn(payload)) {
            return *signedIn;
        }
    }
    const auto error = type == message_type::Error ? lobby::decodeError(payload) : std::nullopt;
    return error ? error->code : ErrorCode::Malformed;
}

} // namespace

std::string describe(ErrorCode code)
{
    switch (code) {
    case ErrorCode::Malformed:
        return "The server did not understand the client";
    case ErrorCode::UnknownMessage:
        return "The server does not know this request";
    case ErrorCode::UnsupportedVersion:
        return "The server runs another version of the game";
    case ErrorCode::HelloRequired:
    case ErrorCode::AlreadyWelcomed:
        return "The client and the server disagree";
    case ErrorCode::InvalidName:
        return "Invalid name: 1 to 16 characters";
    case ErrorCode::GameNotFound:
        return "This game no longer exists";
    case ErrorCode::GameFull:
        return "The game is full";
    case ErrorCode::AlreadyInGame:
        return "Already in a game";
    case ErrorCode::NotInGame:
        return "Not in a game";
    case ErrorCode::ServerFull:
        return "The server cannot host another game";
    case ErrorCode::GameStartFailed:
        return "The game could not start";
    case ErrorCode::Kicked:
        return "An administrator removed you from the server";
    case ErrorCode::Banned:
        return "You are banned from this server";
    case ErrorCode::AccountsDisabled:
        return "This server does not offer this way of signing in";
    case ErrorCode::UsernameTaken:
        return "This username is taken";
    case ErrorCode::WeakPassword:
        return "The password needs 10 to 128 characters, and must not be the username or the email";
    case ErrorCode::InvalidCredentials:
        return "Wrong username, email or password";
    case ErrorCode::EmailNotVerified:
        return "Your email is not verified: a new code was sent";
    case ErrorCode::InvalidCode:
        return "Wrong code";
    case ErrorCode::CodeExpired:
        return "This code expired: ask for a new one";
    case ErrorCode::TooManyAttempts:
        return "Too many attempts: try again later";
    case ErrorCode::SessionExpired:
        return "Your session expired: sign in again";
    case ErrorCode::Busy:
        return "The server is busy: try again in a moment";
    case ErrorCode::GuestsDisabled:
        return "This server needs an account to play";
    case ErrorCode::NameReserved:
        return "This name belongs to an account";
    case ErrorCode::MailUnavailable:
        return "The server cannot send emails now";
    case ErrorCode::DiscordNotConfirmed:
        return "Confirm with Discord first";
    case ErrorCode::InvalidEmail:
        return "Invalid email address";
    case ErrorCode::NotSignedIn:
        return "Not signed in";
    case ErrorCode::AlreadySignedIn:
        return "Already signed in";
    case ErrorCode::DiscordTaken:
        return "This Discord account belongs to another account";
    case ErrorCode::DiscordUnavailable:
        return "The server cannot reach Discord now: try again in a moment";
    case ErrorCode::NotFound:
        return "Nothing was found";
    case ErrorCode::DiscordTokenRefused:
        return "Discord refused the token: sign in with Discord again";
    }
    return std::format("The server refused (error {})", static_cast<int>(code));
}

std::string_view errorKey(ErrorCode code)
{
    switch (code) {
    case ErrorCode::Malformed:
        return "error.malformed";
    case ErrorCode::UnknownMessage:
        return "error.unknown_message";
    case ErrorCode::UnsupportedVersion:
        return "error.unsupported_version";
    case ErrorCode::HelloRequired:
        return "error.hello_required";
    case ErrorCode::AlreadyWelcomed:
        return "error.already_welcomed";
    case ErrorCode::InvalidName:
        return "error.invalid_name";
    case ErrorCode::GameNotFound:
        return "error.game_not_found";
    case ErrorCode::GameFull:
        return "error.game_full";
    case ErrorCode::AlreadyInGame:
        return "error.already_in_game";
    case ErrorCode::NotInGame:
        return "error.not_in_game";
    case ErrorCode::ServerFull:
        return "error.server_full";
    case ErrorCode::GameStartFailed:
        return "error.game_start_failed";
    case ErrorCode::Kicked:
        return "error.kicked";
    case ErrorCode::Banned:
        return "error.banned";
    case ErrorCode::AccountsDisabled:
        return "error.accounts_disabled";
    case ErrorCode::UsernameTaken:
        return "error.username_taken";
    case ErrorCode::WeakPassword:
        return "error.weak_password";
    case ErrorCode::InvalidCredentials:
        return "error.invalid_credentials";
    case ErrorCode::EmailNotVerified:
        return "error.email_not_verified";
    case ErrorCode::InvalidCode:
        return "error.invalid_code";
    case ErrorCode::CodeExpired:
        return "error.code_expired";
    case ErrorCode::TooManyAttempts:
        return "error.too_many_attempts";
    case ErrorCode::SessionExpired:
        return "error.session_expired";
    case ErrorCode::Busy:
        return "error.busy";
    case ErrorCode::GuestsDisabled:
        return "error.guests_disabled";
    case ErrorCode::NameReserved:
        return "error.name_reserved";
    case ErrorCode::MailUnavailable:
        return "error.mail_unavailable";
    case ErrorCode::DiscordNotConfirmed:
        return "error.discord_not_confirmed";
    case ErrorCode::InvalidEmail:
        return "error.invalid_email";
    case ErrorCode::NotSignedIn:
        return "error.not_signed_in";
    case ErrorCode::AlreadySignedIn:
        return "error.already_signed_in";
    case ErrorCode::DiscordTaken:
        return "error.discord_taken";
    case ErrorCode::DiscordUnavailable:
        return "error.discord_unavailable";
    case ErrorCode::NotFound:
        return "error.not_found";
    case ErrorCode::DiscordTokenRefused:
        return "error.discord_token_refused";
    }
    return "error.unknown";
}

LobbyClient::LobbyClient(network::EventLoop &loop, const network::Address &server,
                         std::vector<network::KeyFingerprint> trusted, std::string name, Handlers handlers,
                         std::string locale, std::optional<lobby::SessionToken> token)
    : _name(std::move(name)), _locale(std::move(locale)), _handlers(std::move(handlers)), _ping(loop),
      _token(token)
{
    // Checked now: HELLO is written once connected, in a handler of the loop, where it must not throw
    if (!_token && !lobby::isValidPlayerName(_name)) {
        throw std::invalid_argument("A player name is 1 to 16 characters of text");
    }
    if (!lobby::isValidLocale(_locale)) {
        throw std::invalid_argument("A locale is 2 to 16 of A-Z a-z 0-9 - _");
    }
    logger().info("Connecting to {}", server);
    network::TcpConnection::connect(
        loop, server,
        [this, alive = std::weak_ptr<bool>(_alive), trusted = std::move(trusted),
         server](std::error_code error, std::optional<network::TcpConnection> tcp) {
            if (alive.expired() || _closed) {
                return;
            }
            if (error || !tcp) {
                fail(std::format("Cannot reach {}: {}", server, error.message()));
                return;
            }
            connected(std::move(*tcp), trusted);
        });
}

LobbyClient::~LobbyClient()
{
    close();
}

void LobbyClient::connected(network::TcpConnection connection, std::vector<network::KeyFingerprint> trusted)
{
    _connection.emplace(network::TlsConnection::client(
        std::move(connection), std::move(trusted),
        [this](const network::KeyFingerprint &presented) { _untrusted = presented; }));
    _connection->start(
        [this](std::uint8_t type, std::span<const std::byte> payload) { received(type, payload); },
        [this](std::error_code reason) { connectionClosed(reason); });
    // Sent at once: it waits for the end of the handshake. With a token, the name is the username
    request(
        message_type::Hello,
        lobby::encode(lobby::HelloMessage{.version = lobby::ProtocolVersion,
                                          .name = _token ? std::string() : _name,
                                          .locale = _locale,
                                          .token = _token}),
        [this](std::uint8_t type, std::span<const std::byte> payload) {
            if (type == message_type::Welcome) {
                if (const auto welcome = lobby::decodeWelcome(payload)) {
                    _playerId = welcome->playerId;
                    _features = welcome->features;
                    _account = welcome->account;
                    if (_account) {
                        _name = _account->username;
                    }
                    logger().info("Welcomed as {} ({}){}", _name, *_playerId, _account ? ", signed in" : "");
                    if (_handlers.onWelcome) {
                        _handlers.onWelcome(*_playerId);
                    }
                    return;
                }
            } else if (const auto error = lobby::decodeError(payload); error && type == message_type::Error) {
                if (error->code == ErrorCode::SessionExpired && _handlers.onSessionExpired) {
                    logger().info("The session of the account expired");
                    close();
                    _handlers.onSessionExpired();
                    return;
                }
                fail(error->code);
                return;
            }
            fail("The server answered HELLO with something else");
        });
}

void LobbyClient::connectionClosed(std::error_code reason)
{
    if (reason != network::NetworkError::UntrustedServer || !_untrusted || !_handlers.onUntrusted ||
        _closed) {
        fail(reasonOf(reason));
        return;
    }
    logger().info("The server presented the key {}, not trusted", _untrusted->toHex());
    close();
    _handlers.onUntrusted(*_untrusted);
}

void LobbyClient::request(std::uint8_t type, std::span<const std::byte> payload,
                          std::function<void(std::uint8_t type, std::span<const std::byte> payload)> reply)
{
    if (_closed || !_connection) {
        return;
    }
    _pending.push_back({type, std::move(reply)});
    _connection->send(type, payload);
    // Anything sent keeps the connection alive: PING only after a silence
    _ping.start(PingInterval,
                [this] { request(message_type::Ping, {}, [](std::uint8_t, std::span<const std::byte>) {}); });
}

void LobbyClient::received(std::uint8_t type, std::span<const std::byte> payload)
{
    if (type == message_type::ServerMessage) {
        const std::optional<lobby::ServerMessageMessage> message = lobby::decodeServerMessage(payload);
        if (!message) {
            fail("The server sent a malformed SERVER_MESSAGE");
            return;
        }
        logger().info("Message from the server: {}", message->text);
        if (_handlers.onServerMessage) {
            _handlers.onServerMessage(message->text);
        }
        return;
    }
    if (type == message_type::Progress) {
        const std::optional<lobby::ProgressMessage> progress = lobby::decodeProgress(payload);
        if (!progress) {
            fail("The server sent a malformed PROGRESS");
            return;
        }
        _progress = progress;
        if (_handlers.onProgress) {
            _handlers.onProgress(*progress);
        }
        return;
    }
    // The Discord token: the reply of DISCORD_REFRESH, or given before the reply of a sign-in with Discord
    if (type == message_type::DiscordToken) {
        const std::optional<lobby::DiscordTokenMessage> token = lobby::decodeDiscordToken(payload);
        if (!token) {
            fail("The server sent a malformed DISCORD_TOKEN");
            return;
        }
        logger().info("The server gave the Discord token of the player");
        if (_handlers.onDiscordToken) {
            _handlers.onDiscordToken(*token);
        }
        if (_pending.empty() || _pending.front().request != message_type::DiscordRefresh) {
            return;
        }
    }
    // An error after which the master closes the connection: KICKED and BANNED without a request, BANNED
    // after a SIGN_IN. The one of HELLO goes to it, for SESSION_EXPIRED
    if (type == message_type::Error) {
        const std::optional<lobby::ErrorMessage> error = lobby::decodeError(payload);
        if (error && lobby::closesConnection(error->code) &&
            (error->request == 0 || _pending.empty() || _pending.front().request != message_type::Hello)) {
            fail(error->code);
            return;
        }
    }
    // LEFT_GAME is a reply to LEAVE_GAME, or a notification when the player did not ask
    if (type == message_type::LeftGame) {
        const std::optional<lobby::LeftGameMessage> left = lobby::decodeLeftGame(payload);
        if (!left) {
            fail("The server sent a malformed LEFT_GAME");
            return;
        }
        const bool reply = !_pending.empty() && _pending.front().request == message_type::LeaveGame &&
                           left->reason == lobby::LeftGameReason::Requested;
        if (!reply) {
            if (_handlers.onLeftGame) {
                _handlers.onLeftGame(*left);
            }
            return;
        }
    }
    if (_pending.empty()) {
        fail(std::format("The server sent message {:#04x} without a request", type));
        return;
    }
    Pending pending = std::move(_pending.front());
    _pending.pop_front();
    if (pending.reply) {
        pending.reply(type, payload);
    }
}

void LobbyClient::listGames(ListCallback done)
{
    request(message_type::ListGames, {},
            [this, done = std::move(done)](std::uint8_t type, std::span<const std::byte> payload) {
                const auto list =
                    type == message_type::GameList ? lobby::decodeGameList(payload) : std::nullopt;
                if (!list) {
                    fail("The server answered LIST_GAMES with something else");
                    return;
                }
                if (done) {
                    done(list->games);
                }
            });
}

void LobbyClient::createGame(const std::string &name, std::uint8_t maxPlayers, JoinCallback done)
{
    request(message_type::CreateGame, lobby::encode(lobby::CreateGameMessage{name, maxPlayers}),
            [done = std::move(done)](std::uint8_t type, std::span<const std::byte> payload) {
                if (type == message_type::GameJoined) {
                    if (const auto joined = lobby::decodeGameJoined(payload)) {
                        done(*joined);
                        return;
                    }
                }
                const auto error = lobby::decodeError(payload);
                done(error ? error->code : ErrorCode::Malformed);
            });
}

void LobbyClient::joinGame(std::uint32_t gameId, JoinCallback done)
{
    request(message_type::JoinGame, lobby::encode(lobby::JoinGameMessage{gameId}),
            [done = std::move(done)](std::uint8_t type, std::span<const std::byte> payload) {
                if (type == message_type::GameJoined) {
                    if (const auto joined = lobby::decodeGameJoined(payload)) {
                        done(*joined);
                        return;
                    }
                }
                const auto error = lobby::decodeError(payload);
                done(error ? error->code : ErrorCode::Malformed);
            });
}

void LobbyClient::leaveGame(std::function<void()> done)
{
    request(message_type::LeaveGame, {}, [done = std::move(done)](std::uint8_t, std::span<const std::byte>) {
        if (done) {
            done();
        }
    });
}

void LobbyClient::signInRequest(std::uint8_t type, std::vector<std::byte> payload, SignInCallback done)
{
    request(type, payload,
            [this, done = std::move(done)](std::uint8_t reply, std::span<const std::byte> body) {
                const SignInResult result = signInResult(reply, body);
                if (const auto *signedIn = std::get_if<lobby::SignedInMessage>(&result)) {
                    _account = signedIn->account;
                    _name = signedIn->account.username;
                    logger().info("Signed in as {}", _name);
                }
                if (done) {
                    done(result);
                }
            });
}

void LobbyClient::doneRequest(std::uint8_t type, std::vector<std::byte> payload, DoneCallback done,
                              bool closes)
{
    request(type, payload,
            [this, done = std::move(done), closes](std::uint8_t reply, std::span<const std::byte> body) {
                std::optional<ErrorCode> error;
                if (reply == message_type::Error) {
                    const auto message = lobby::decodeError(body);
                    error = message ? message->code : ErrorCode::Malformed;
                } else if ((reply != message_type::CodeSent && reply != message_type::Done) ||
                           !lobby::decodeEmpty(body)) {
                    error = ErrorCode::Malformed;
                }
                if (!error && closes) {
                    // The master closes the connection after DONE: not a connection lost
                    logger().info("Signed out of {}", _name);
                    _account.reset();
                    _progress.reset();
                    close();
                }
                if (done) {
                    done(error);
                }
            });
}

void LobbyClient::registerAccount(const std::string &username, const std::string &email,
                                  const std::string &password, DoneCallback done)
{
    if (!fits(username, lobby::MaxUsernameLength)) {
        done(ErrorCode::InvalidName);
    } else if (!fits(email, lobby::MaxEmailLength)) {
        done(ErrorCode::InvalidEmail);
    } else if (!fits(password, lobby::MaxPasswordLength)) {
        done(ErrorCode::WeakPassword);
    } else {
        doneRequest(message_type::Register, lobby::encode(lobby::RegisterMessage{username, email, password}),
                    std::move(done));
    }
}

void LobbyClient::verifyEmail(const std::string &login, const std::string &code, SignInCallback done)
{
    if (!fits(login, lobby::MaxEmailLength) || !lobby::isValidEmailCode(code)) {
        done(ErrorCode::InvalidCode);
        return;
    }
    signInRequest(message_type::VerifyEmail, lobby::encode(lobby::VerifyEmailMessage{login, code}),
                  std::move(done));
}

void LobbyClient::resendCode(const std::string &login, DoneCallback done)
{
    if (!fits(login, lobby::MaxEmailLength)) {
        done(ErrorCode::InvalidName);
        return;
    }
    doneRequest(message_type::ResendCode, lobby::encode(lobby::ResendCodeMessage{login}), std::move(done));
}

void LobbyClient::signIn(const std::string &login, const std::string &password, SignInCallback done)
{
    if (!fits(login, lobby::MaxEmailLength) || !fits(password, lobby::MaxPasswordLength)) {
        done(ErrorCode::InvalidCredentials);
        return;
    }
    signInRequest(message_type::SignIn, lobby::encode(lobby::SignInMessage{login, password}),
                  std::move(done));
}

void LobbyClient::forgotPassword(const std::string &email, DoneCallback done)
{
    if (!fits(email, lobby::MaxEmailLength)) {
        done(ErrorCode::InvalidEmail);
        return;
    }
    doneRequest(message_type::ForgotPassword, lobby::encode(lobby::ForgotPasswordMessage{email}),
                std::move(done));
}

void LobbyClient::resetPassword(const std::string &email, const std::string &code,
                                const std::string &password, SignInCallback done)
{
    if (!fits(email, lobby::MaxEmailLength)) {
        done(ErrorCode::InvalidEmail);
    } else if (!lobby::isValidEmailCode(code)) {
        done(ErrorCode::InvalidCode);
    } else if (!fits(password, lobby::MaxPasswordLength)) {
        done(ErrorCode::WeakPassword);
    } else {
        signInRequest(message_type::ResetPassword,
                      lobby::encode(lobby::ResetPasswordMessage{email, code, password}), std::move(done));
    }
}

void LobbyClient::signOut(DoneCallback done)
{
    doneRequest(message_type::SignOut, {}, std::move(done), true);
}

void LobbyClient::deleteAccount(const std::string &password, DoneCallback done)
{
    // Empty after a Discord confirmation, for an account without password
    if (!fits(password, lobby::MaxPasswordLength, true)) {
        done(ErrorCode::InvalidCredentials);
        return;
    }
    doneRequest(message_type::DeleteAccount, lobby::encode(lobby::DeleteAccountMessage{password}),
                std::move(done), true);
}

void LobbyClient::profile(std::uint64_t accountId, ProfileCallback done)
{
    request(message_type::GetProfile, lobby::encode(lobby::GetProfileMessage{accountId}),
            [accountId, done = std::move(done)](std::uint8_t type, std::span<const std::byte> payload) {
                if (type == message_type::Profile) {
                    if (const auto profile = lobby::decodeProfile(payload);
                        profile && profile->accountId == accountId) {
                        done(*profile);
                        return;
                    }
                }
                const auto error = type == message_type::Error ? lobby::decodeError(payload) : std::nullopt;
                done(error ? error->code : ErrorCode::Malformed);
            });
}

void LobbyClient::picture(std::uint64_t accountId, PictureCallback done)
{
    picturePart(accountId, std::make_shared<Picture>(), 0, std::move(done));
}

void LobbyClient::picturePart(std::uint64_t accountId, std::shared_ptr<Picture> picture, std::uint32_t total,
                              PictureCallback done)
{
    const auto offset = static_cast<std::uint32_t>(picture->png.size());
    request(message_type::GetAvatar, lobby::encode(lobby::GetAvatarMessage{accountId, offset}),
            [this, accountId, picture, total, offset,
             done = std::move(done)](std::uint8_t type, std::span<const std::byte> payload) {
                if (type != message_type::Avatar) {
                    const auto error =
                        type == message_type::Error ? lobby::decodeError(payload) : std::nullopt;
                    done(error ? error->code : ErrorCode::Malformed);
                    return;
                }
                const auto part = lobby::decodeAvatar(payload);
                if (!part || part->accountId != accountId || part->offset != offset) {
                    done(ErrorCode::Malformed);
                    return;
                }
                // The picture changed between two parts: what was read is of another one
                if (offset > 0 && (part->avatarHash != picture->hash || part->total != total)) {
                    done(ErrorCode::NotFound);
                    return;
                }
                picture->hash = part->avatarHash;
                picture->png.insert(picture->png.end(), part->bytes.begin(), part->bytes.end());
                if (picture->png.size() >= part->total) {
                    done(*picture);
                    return;
                }
                picturePart(accountId, picture, part->total, done);
            });
}

void LobbyClient::discordSignIn(DiscordAuthorizeCallback done)
{
    request(message_type::DiscordSignIn, {},
            [done = std::move(done)](std::uint8_t type, std::span<const std::byte> payload) {
                if (type == message_type::DiscordAuthorize) {
                    if (const auto authorize = lobby::decodeDiscordAuthorize(payload)) {
                        done(*authorize);
                        return;
                    }
                }
                const auto error = type == message_type::Error ? lobby::decodeError(payload) : std::nullopt;
                done(error ? error->code : ErrorCode::Malformed);
            });
}

void LobbyClient::discordRefresh(const std::string &refreshToken, DoneCallback done)
{
    if (!lobby::isValidDiscordToken(refreshToken)) {
        done(ErrorCode::DiscordTokenRefused);
        return;
    }
    request(message_type::DiscordRefresh, lobby::encode(lobby::DiscordRefreshMessage{refreshToken}),
            [done = std::move(done)](std::uint8_t type, std::span<const std::byte> payload) {
                std::optional<ErrorCode> error;
                if (type == message_type::Error) {
                    const auto message = lobby::decodeError(payload);
                    error = message ? message->code : ErrorCode::Malformed;
                } else if (type != message_type::DiscordToken) {
                    error = ErrorCode::Malformed;
                }
                if (done) {
                    done(error);
                }
            });
}

void LobbyClient::discordConfirm(const lobby::DiscordConfirmMessage &confirmation,
                                 DiscordConfirmCallback done)
{
    if (!fits(confirmation.code, lobby::MaxDiscordCodeLength) ||
        !lobby::isValidCodeVerifier(confirmation.codeVerifier) ||
        !fits(confirmation.redirectUri, lobby::MaxRedirectUriLength)) {
        done(ErrorCode::InvalidCode);
        return;
    }
    request(message_type::DiscordConfirm, lobby::encode(confirmation),
            [this, done = std::move(done)](std::uint8_t type, std::span<const std::byte> payload) {
                if (type == message_type::Done && lobby::decodeEmpty(payload)) {
                    done(DiscordConfirmed{});
                    return;
                }
                if (type == message_type::Profile) {
                    if (const auto profile = lobby::decodeProfile(payload);
                        profile && profile->accountId == 0) {
                        done(DiscordConfirmed{.newUser = *profile});
                        return;
                    }
                }
                const SignInResult result = signInResult(type, payload);
                if (const auto *signedIn = std::get_if<lobby::SignedInMessage>(&result)) {
                    _account = signedIn->account;
                    _name = signedIn->account.username;
                    logger().info("Signed in with Discord as {}", _name);
                    done(*signedIn);
                    return;
                }
                done(std::get<ErrorCode>(result));
            });
}

void LobbyClient::chooseUsername(const std::string &username, SignInCallback done)
{
    if (!fits(username, lobby::MaxUsernameLength)) {
        done(ErrorCode::InvalidName);
        return;
    }
    signInRequest(message_type::ChooseUsername, lobby::encode(lobby::ChooseUsernameMessage{username}),
                  std::move(done));
}

void LobbyClient::fail(ErrorCode code)
{
    fail(_handlers.describe ? _handlers.describe(code) : describe(code));
}

void LobbyClient::fail(const std::string &reason)
{
    if (_closed) {
        return;
    }
    logger().warning("{}", reason);
    close();
    if (_handlers.onClosed) {
        _handlers.onClosed(reason);
    }
}

void LobbyClient::close()
{
    if (_closed) {
        return;
    }
    _closed = true;
    _ping.cancel();
    _pending.clear();
    if (_connection) {
        _connection->close();
    }
}

} // namespace rtype::client
