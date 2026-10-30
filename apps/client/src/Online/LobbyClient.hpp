#pragma once

#include "rtype/lobby/Messages.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/FramedConnection.hpp"
#include "rtype/network/Timer.hpp"
#include "rtype/network/Tls.hpp"

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>
#include <vector>

namespace rtype::client
{

/// What an error of the master means, for the player, in English: "The game is full".
[[nodiscard]] std::string describe(lobby::ErrorCode code);
/// The key of the message of an error of the master in the catalogs of the client (`en.lang`, `fr.lang`):
/// "error.game_full", "error.unknown" for a code the client does not know.
[[nodiscard]] std::string_view errorKey(lobby::ErrorCode code);

/// The connection of the client to the lobby of a master (docs/protocol/lobby.md): TCP, TLS with the keys
/// trusted for the server pinned, then HELLO, then the requests of the player.
///
/// @code
/// LobbyClient lobby(loop, server, {pinned}, "Pavel",
///                   {.onWelcome = [&](std::uint32_t player) { lobby.listGames(...); },
///                    .onLeftGame = [&](const lobby::LeftGameMessage &left) { /* back to the list */ },
///                    .onClosed = [&](const std::string &reason) { /* toast it */ }});
/// lobby.joinGame(id, [&](const LobbyClient::JoinResult &result) { /* GAME_JOINED or an error */ });
/// @endcode
///
/// - The master answers the requests in order: each reply goes to the callback of its request.
/// - A server that does not have a trusted key is refused during the handshake, before HELLO leaves:
/// onUntrusted
///   tells the key it presented, so that the player can trust it (ADR 0021), or onClosed without it.
/// - After 10 seconds without sending anything, it sends PING, so that the master keeps the connection.
/// - SERVER_MESSAGE, and LEFT_GAME when the player did not ask, are notifications: they go to their handlers.
///   ERROR (KICKED) or (BANNED) without a request closes the connection, with its reason.
/// - With a session token, HELLO signs the account in (lobby protocol 3): WELCOME gives the account, or
///   SESSION_EXPIRED goes to onSessionExpired, so that the client forgets the token and connects again.
/// - The requests of the accounts check their fields before sending: a field the protocol cannot carry gets
/// the
///   error the master would give, at once. A password, a code and a token are never logged.
/// - Everything runs on the thread that polls the loop: in the client, the game loop.
class LobbyClient
{
public:
    using JoinResult = std::variant<lobby::GameJoinedMessage, lobby::ErrorCode>;
    using ListCallback = std::function<void(const std::vector<lobby::GameInfo> &games)>;
    using JoinCallback = std::function<void(const JoinResult &result)>;
    /// SIGNED_IN, or the error of the master.
    using SignInResult = std::variant<lobby::SignedInMessage, lobby::ErrorCode>;
    using SignInCallback = std::function<void(const SignInResult &result)>;
    /// Nothing for CODE_SENT or DONE, else the error of the master.
    using DoneCallback = std::function<void(std::optional<lobby::ErrorCode> error)>;

    struct Handlers {
        /// The master welcomed the player: requests can be sent.
        std::function<void(std::uint32_t playerId)> onWelcome;
        /// The player is no longer in its game without having asked: game over, crash, timeout, an
        /// administrator.
        std::function<void(const lobby::LeftGameMessage &left)> onLeftGame;
        /// The administrators of the server sent a message (SERVER_MESSAGE).
        std::function<void(const std::string &text)> onServerMessage;
        /// The connection ended, or could not be opened, with the reason for the player. Called once; nothing
        /// is called after.
        std::function<void(const std::string &reason)> onClosed;
        /// The server presented a key that is not trusted, which is given: called instead of onClosed if set.
        std::function<void(const network::KeyFingerprint &presented)> onUntrusted;
        /// The session token of HELLO is unknown or expired: the connection is closed, the client forgets the
        /// token and connects again without it. Called instead of onClosed if set.
        std::function<void()> onSessionExpired;
        /// What an error of the master that closes the connection means, for onClosed: describe() if empty.
        std::function<std::string(lobby::ErrorCode code)> describe;
    };

    /// Sent PING after this long without sending anything (docs/protocol/lobby.md, PING).
    static constexpr std::chrono::seconds PingInterval{10};

private:
    struct Pending {
        std::uint8_t request = 0;
        std::function<void(std::uint8_t type, std::span<const std::byte> payload)> reply;
    };

    std::string _name;
    std::string _locale;
    Handlers _handlers;
    std::optional<network::FramedConnection> _connection;
    network::Timer _ping;
    std::deque<Pending> _pending;
    std::optional<lobby::SessionToken> _token;
    std::optional<std::uint32_t> _playerId;
    lobby::ServerFeatures _features;
    std::optional<lobby::AccountInfo> _account;
    bool _closed = false;
    /// The key the server presented, if it was refused.
    std::optional<network::KeyFingerprint> _untrusted;
    std::shared_ptr<bool> _alive = std::make_shared<bool>(true);

    void connected(network::TcpConnection connection, std::vector<network::KeyFingerprint> trusted);
    void connectionClosed(std::error_code reason);
    void received(std::uint8_t type, std::span<const std::byte> payload);
    void request(std::uint8_t type, std::span<const std::byte> payload,
                 std::function<void(std::uint8_t type, std::span<const std::byte> payload)> reply);
    void fail(const std::string &reason);
    /// Fails with what the error means for the player.
    void fail(lobby::ErrorCode code);
    /// A request answered SIGNED_IN or an error: signs the connection in on SIGNED_IN.
    void signInRequest(std::uint8_t type, std::vector<std::byte> payload, SignInCallback done);
    /// A request answered CODE_SENT, DONE or an error. `closes`: the master closes the connection after DONE.
    void doneRequest(std::uint8_t type, std::vector<std::byte> payload, DoneCallback done,
                     bool closes = false);

public:
    /// Starts connecting at once, without blocking.
    /// @param trusted The keys the server may present (KnownServers::trusted()). None: the server is refused,
    /// and onUntrusted tells its key.
    /// @param name The name of a guest. Ignored with a token: the name is then the username of the account.
    /// @param locale The language of the player, sent in HELLO: the master writes its emails in it.
    /// @param token The session token of an account on this server (SessionTokens), to come back signed in.
    /// @throws std::invalid_argument if the name cannot be the name of a player (lobby::isValidPlayerName())
    /// without a token, or the locale cannot be a locale (lobby::isValidLocale()).
    LobbyClient(network::EventLoop &loop, const network::Address &server,
                std::vector<network::KeyFingerprint> trusted, std::string name, Handlers handlers,
                std::string locale = "en", std::optional<lobby::SessionToken> token = std::nullopt);
    ~LobbyClient();

    LobbyClient(const LobbyClient &) = delete;
    LobbyClient &operator=(const LobbyClient &) = delete;

    /// The id the master gave the player, once welcomed.
    [[nodiscard]] std::optional<std::uint32_t> playerId() const { return _playerId; }
    [[nodiscard]] bool welcomed() const { return _playerId.has_value() && !_closed; }
    /// The name of the player: the username of the account once signed in.
    [[nodiscard]] const std::string &name() const { return _name; }
    /// What the master offers, once welcomed.
    [[nodiscard]] const lobby::ServerFeatures &features() const { return _features; }
    /// The account signed in on this connection, if any: by the token of HELLO, or by SIGNED_IN.
    [[nodiscard]] const std::optional<lobby::AccountInfo> &account() const { return _account; }

    void listGames(ListCallback done);
    void createGame(const std::string &name, std::uint8_t maxPlayers, JoinCallback done);
    void joinGame(std::uint32_t gameId, JoinCallback done);
    /// LEAVE_GAME. `done` is called once the master answered, whatever it answered.
    void leaveGame(std::function<void()> done = {});

    // The accounts (docs/protocol/lobby.md, section 6). A field the protocol cannot carry gets, at once, the
    // error the master would give it: INVALID_NAME, INVALID_EMAIL, WEAK_PASSWORD, INVALID_CREDENTIALS or
    // INVALID_CODE.

    /// REGISTER: CODE_SENT, whether the email has an account or not.
    void registerAccount(const std::string &username, const std::string &email, const std::string &password,
                         DoneCallback done);
    /// VERIFY_EMAIL, with the username or the email of the account.
    void verifyEmail(const std::string &login, const std::string &code, SignInCallback done);
    /// RESEND_CODE: a new code to verify the email.
    void resendCode(const std::string &login, DoneCallback done);
    /// SIGN_IN, with the username or the email of the account.
    void signIn(const std::string &login, const std::string &password, SignInCallback done);
    /// FORGOT_PASSWORD: CODE_SENT, whether the email has an account or not.
    void forgotPassword(const std::string &email, DoneCallback done);
    /// RESET_PASSWORD: the code of FORGOT_PASSWORD and the new password, which signs in.
    void resetPassword(const std::string &email, const std::string &code, const std::string &password,
                       SignInCallback done);
    /// SIGN_OUT. On DONE, the master closes the connection: it is closed here first, without onClosed.
    void signOut(DoneCallback done);
    /// DELETE_ACCOUNT, with the password of the account. On DONE, closed as for signOut().
    void deleteAccount(const std::string &password, DoneCallback done);

    /// Closes the connection, after what was sent. No handler is called after.
    void close();
};

} // namespace rtype::client
