#pragma once

#include "Game/GameScenes.hpp"
#include "Online/LobbyClient.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/lobby/Messages.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rtype::client
{

/// Name of the list of the games of a server.
inline constexpr const char *LobbyScene = "online_lobby";

/// Splits "host:port" into its host and its port, the default port without one. IPv6 addresses go in
/// brackets: "[::1]:4242".
/// @return Nothing if the port is not a number from 1 to 65535.
[[nodiscard]] std::optional<std::pair<std::string, std::uint16_t>>
splitServer(const std::string &text, std::uint16_t defaultPort = 4242);

/// Playing online (docs/architecture/client-online.md): the Online menu, where the player gives the server
/// and its name, then the lobby of the server, with its games.
///
/// - **Connect** opens a LobbyClient to the server, with the key of `server.pub`, next to the executable: a
///   server without that key is refused.
/// - **The lobby** lists the games: Create a game, Join one, Refresh, Disconnect. Joining plays a
///   RemoteSession in the game scene; when the game ends, the player is back in the lobby.
/// - What goes wrong is shown in a toast: the server cannot be reached, is not the expected one, the game is
///   full...
class OnlineScenes
{
    engine::Engine &_engine;
    network::EventLoop &_loop;
    GameScenes &_games;
    std::filesystem::path _publicKey;
    std::string _serverText = "127.0.0.1:4242";
    std::string _name = "Player";
    std::optional<network::Address> _server;
    std::unique_ptr<LobbyClient> _lobby;
    std::vector<lobby::GameInfo> _list;

    void connect();
    void disconnect();
    /// Lists the games, then shows them.
    void refresh();
    void joined(const LobbyClient::JoinResult &result);
    /// The game played ends: leaves it, then back to the list.
    void gameEnded();
    void lobbyClosed(const std::string &reason);
    /// Forgets the lobby later: this may run in one of its handlers.
    void forgetLobby();

public:
    /// Declares the Online menu and the lobby. Everything given must outlive the engine.
    /// @param publicKey The public key of the server that the client pins.
    OnlineScenes(engine::Engine &engine, network::EventLoop &loop, GameScenes &games,
                 const engine::Metrics &metrics, const engine::InterfaceStyle &style,
                 std::filesystem::path publicKey);

    OnlineScenes(const OnlineScenes &) = delete;
    OnlineScenes &operator=(const OnlineScenes &) = delete;
};

} // namespace rtype::client
