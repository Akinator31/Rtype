#pragma once

#include "Game/GameScenes.hpp"
#include "Online/KnownServers.hpp"
#include "Online/LobbyClient.hpp"
#include "Online/SessionTokens.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/lobby/Messages.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/Tls.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rtype::client
{

/// Name of the list of the games of a server.
inline constexpr const char *LobbyScene = "online_lobby";
/// Name of the question asked at the first connection to a server: trust its key?
inline constexpr const char *TrustServerScene = "online_trust";
/// Name of the warning shown when a known server presents another key.
inline constexpr const char *ChangedKeyScene = "online_changed_key";
/// Names of the scenes of the accounts (docs/architecture/client-online.md#accounts): what the server offers,
/// then signing in, creating an account, the code of the email, the forgotten password, and the account
/// signed in.
inline constexpr const char *AccountsScene = "online_accounts";
inline constexpr const char *SignInScene = "online_sign_in";
inline constexpr const char *RegisterScene = "online_register";
inline constexpr const char *CodeScene = "online_code";
inline constexpr const char *ForgotPasswordScene = "online_forgot_password";
inline constexpr const char *ResetPasswordScene = "online_reset_password";
inline constexpr const char *MyAccountScene = "online_my_account";
inline constexpr const char *DeleteAccountScene = "online_delete_account";

/// A fingerprint as shown to the player: its 64 digits in groups of 4, `groupsPerLine` groups per line.
[[nodiscard]] std::vector<std::string> fingerprintLines(const network::KeyFingerprint &fingerprint,
                                                        std::size_t groupsPerLine = 8);

/// Splits "host:port" into its host and its port, the default port without one. IPv6 addresses go in
/// brackets: "[::1]:4242".
/// @return Nothing if the port is not a number from 1 to 65535.
[[nodiscard]] std::optional<std::pair<std::string, std::uint16_t>>
splitServer(const std::string &text, std::uint16_t defaultPort = 4242);

/// Playing online (docs/architecture/client-online.md): the Online menu, where the player gives a server and
/// its name, then the lobby of the server, with its games.
///
/// - **Connect** opens a LobbyClient to the server, trusting the key KnownServers knows for it (ADR 0021).
///   A server never met shows its key, which the player may trust: it is remembered, and the client connects
///   again. A known server with another key is refused, with a warning, until the player forgets its key
///   (**Forget the server key**).
/// - **The lobby** lists the games: Create a game, Join one, Refresh, Disconnect. Joining plays a
///   RemoteSession in the game scene; when the game ends, the player is back in the lobby.
/// - **The accounts**: on a server that offers them, WELCOME leads to a choice (sign in, create an account,
///   play as a guest). The session token of SIGNED_IN is kept for the server (SessionTokens) and sent in the
///   next HELLO: the player comes back signed in. It is forgotten at sign-out, at the deletion of the
///   account, and on SESSION_EXPIRED, after which the client connects again as a guest. A server without
///   accounts looks as before them.
/// - What goes wrong is shown in a toast, in the language of the game: the server cannot be reached, is not
///   the expected one, the game is full...
class OnlineScenes
{
    engine::Engine &_engine;
    network::EventLoop &_loop;
    GameScenes &_games;
    localization::Localization &_translations;
    KnownServers _known;
    SessionTokens _sessions;
    std::string _serverText = "127.0.0.1:4242";
    std::string _name = "Player";
    std::optional<network::Address> _server;
    /// The server connected to, as KnownServers knows it.
    std::string _address;
    /// The key a server presented and the client did not trust, shown to the player.
    std::optional<network::KeyFingerprint> _presented;
    std::unique_ptr<LobbyClient> _lobby;
    std::vector<lobby::GameInfo> _list;
    /// The username or email of the account whose email is verified with a code, in CodeScene.
    std::string _login;
    /// The email of the forgotten password, in ResetPasswordScene.
    std::string _email;

    void connect();
    /// The server presented a key not trusted: asks the player, or warns it.
    void untrusted(const network::KeyFingerprint &presented);
    /// Forgets the key of the server typed, if the player trusted one.
    void forgetServer();
    void disconnect();
    /// Lists the games, then shows them.
    void refresh();
    void joined(const LobbyClient::JoinResult &result);
    /// The game played ends: leaves it, then back to the list.
    void gameEnded();
    void lobbyClosed(const std::string &reason);
    /// Forgets the lobby later: this may run in one of its handlers.
    void forgetLobby();

    /// What an error of the master means, in the language of the game.
    [[nodiscard]] std::string message(lobby::ErrorCode code) const;
    void declareAccountScenes(const engine::Metrics &metrics, const engine::InterfaceStyle &style);
    /// WELCOME: the choice of the accounts if the server offers them and the player is not signed in, else
    /// the games.
    void welcomed();
    /// The answer to a request that signs in: keeps the token, then the games.
    void signedIn(const LobbyClient::SignInResult &result);
    /// The answer to SIGN_OUT or DELETE_ACCOUNT: forgets the token, then connects again as a guest.
    void signedOut(std::optional<lobby::ErrorCode> error, const char *done);

public:
    /// Declares the Online menu and the lobby. Everything given must outlive the engine.
    /// The servers trusted are kept in the settings of the engine.
    OnlineScenes(engine::Engine &engine, network::EventLoop &loop, GameScenes &games,
                 const engine::Metrics &metrics, const engine::InterfaceStyle &style,
                 localization::Localization &translations);

    OnlineScenes(const OnlineScenes &) = delete;
    OnlineScenes &operator=(const OnlineScenes &) = delete;
};

} // namespace rtype::client
