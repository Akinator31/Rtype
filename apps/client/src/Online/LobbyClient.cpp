#include "Online/LobbyClient.hpp"

#include "rtype/log/Logger.hpp"
#include "rtype/network/Errors.hpp"
#include "rtype/network/TcpConnection.hpp"
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
    }
    return std::format("The server refused (error {})", static_cast<int>(code));
}

LobbyClient::LobbyClient(network::EventLoop &loop, const network::Address &server,
                         std::vector<network::KeyFingerprint> trusted, std::string name, Handlers handlers,
                         std::string locale)
    : _name(std::move(name)), _locale(std::move(locale)), _handlers(std::move(handlers)), _ping(loop)
{
    // Checked now: HELLO is written once connected, in a handler of the loop, where it must not throw
    if (!lobby::isValidPlayerName(_name)) {
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
    // Sent at once: it waits for the end of the handshake
    request(message_type::Hello,
            lobby::encode(lobby::HelloMessage{
                .version = lobby::ProtocolVersion, .name = _name, .locale = _locale, .token = std::nullopt}),
            [this](std::uint8_t type, std::span<const std::byte> payload) {
                if (type == message_type::Welcome) {
                    if (const auto welcome = lobby::decodeWelcome(payload)) {
                        _playerId = welcome->playerId;
                        _features = welcome->features;
                        logger().info("Welcomed as {} ({})", _name, *_playerId);
                        if (_handlers.onWelcome) {
                            _handlers.onWelcome(*_playerId);
                        }
                        return;
                    }
                } else if (const auto error = lobby::decodeError(payload);
                           error && type == message_type::Error) {
                    fail(describe(error->code));
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
    // KICKED and BANNED come without a request, then the master closes the connection
    if (type == message_type::Error) {
        const std::optional<lobby::ErrorMessage> error = lobby::decodeError(payload);
        if (error && error->request == 0 &&
            (error->code == ErrorCode::Kicked || error->code == ErrorCode::Banned)) {
            fail(describe(error->code));
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
