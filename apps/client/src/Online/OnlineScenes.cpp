#include "Online/OnlineScenes.hpp"

#include "Game/RemoteSession.hpp"
#include "MainMenu/MainMenu.hpp"
#include "MainMenu/Submenus.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/engine/Interface/MenuBuilder.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/Tls.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace rtype::client
{

namespace
{

/// Most games listed: the others are reached with Refresh once some end.
constexpr std::size_t MaxGamesShown = 6;
/// Players of a game created from the client.
constexpr std::uint8_t CreatedGamePlayers = 4;

const log::Logger &logger()
{
    static const log::Logger instance("online");
    return instance;
}

std::string leftReason(lobby::LeftGameReason reason)
{
    switch (reason) {
    case lobby::LeftGameReason::Requested:
        return "Left the game";
    case lobby::LeftGameReason::GameOver:
        return "The game is over";
    case lobby::LeftGameReason::GameCrashed:
        return "The game stopped unexpectedly";
    case lobby::LeftGameReason::TimedOut:
        return "The game lost the connection with the client";
    }
    return "Left the game";
}

} // namespace

std::optional<std::pair<std::string, std::uint16_t>> splitServer(const std::string &text,
                                                                 std::uint16_t defaultPort)
{
    std::string host = text;
    std::string port;
    if (!text.empty() && text.front() == '[') {
        // [IPv6]:port
        const std::size_t close = text.find(']');
        if (close == std::string::npos) {
            return std::nullopt;
        }
        host = text.substr(1, close - 1);
        if (close + 1 < text.size()) {
            if (text[close + 1] != ':') {
                return std::nullopt;
            }
            port = text.substr(close + 2);
        }
    } else if (const std::size_t colon = text.rfind(':');
               colon != std::string::npos && text.find(':') == colon) {
        host = text.substr(0, colon);
        port = text.substr(colon + 1);
    }
    if (host.empty()) {
        return std::nullopt;
    }
    if (port.empty()) {
        return std::pair{host, defaultPort};
    }
    unsigned value = 0;
    const auto [end, error] = std::from_chars(port.data(), port.data() + port.size(), value);
    if (error != std::errc{} || end != port.data() + port.size() || value == 0 || value > 65535) {
        return std::nullopt;
    }
    return std::pair{host, static_cast<std::uint16_t>(value)};
}

OnlineScenes::OnlineScenes(engine::Engine &engine, network::EventLoop &loop, GameScenes &games,
                           const engine::Metrics &metrics, const engine::InterfaceStyle &style,
                           std::filesystem::path publicKey)
    : _engine(engine), _loop(loop), _games(games), _publicKey(std::move(publicKey))
{
    engine.createScene(OnlineMenuScene, {.overlay = true, .hidesInterfaceBelow = true},
                       [this, &metrics, &style](engine::Engine &target, engine::SceneId scene) {
                           engine::MenuBuilder menu(target.getEcs(), scene, metrics, style, "Online", true);
                           const ecs::entity_t server = menu.textField("Server", _serverText, {}, 64);
                           const ecs::entity_t name =
                               menu.textField("Name", _name, {}, lobby::MaxPlayerNameLength);
                           menu.button("Connect", [this, &target, server, name] {
                               _serverText =
                                   target.getEcs().getComponent<engine::TextField>(server).input.text();
                               _name = target.getEcs().getComponent<engine::TextField>(name).input.text();
                               connect();
                           });
                           menu.button("Back", [&target] { target.popScene(); });
                       });

    engine.createScene(
        LobbyScene, {.onBack = [this] { disconnect(); }},
        [this, &metrics, &style](engine::Engine &target, engine::SceneId scene) {
            engine::MenuBuilder menu(target.getEcs(), scene, metrics, style, "Online");
            menu.label(std::format("{} on {}", _name, _serverText));
            menu.button("Create a game", [this] {
                if (_lobby) {
                    const std::string game =
                        std::string(_name).append("'s game").substr(0, lobby::MaxGameNameLength);
                    _lobby->createGame(game, CreatedGamePlayers,
                                       [this](const LobbyClient::JoinResult &result) { joined(result); });
                }
            });
            for (std::size_t i = 0; i < std::min(_list.size(), MaxGamesShown); ++i) {
                const lobby::GameInfo &game = _list[i];
                menu.button(std::format("Join {} ({}/{})", game.name, game.players, game.maxPlayers),
                            [this, id = game.id] {
                                if (_lobby) {
                                    _lobby->joinGame(id, [this](const LobbyClient::JoinResult &result) {
                                        joined(result);
                                    });
                                }
                            });
            }
            if (_list.empty()) {
                menu.label("No game yet: create one");
            }
            menu.button("Refresh", [this] { refresh(); });
            menu.button("Disconnect", [this] { disconnect(); });
        });
}

void OnlineScenes::connect()
{
    if (!lobby::isValidPlayerName(_name)) {
        _engine.showToast("Invalid name: 1 to 16 characters", engine::ToastType::Error);
        return;
    }
    const auto parts = splitServer(_serverText);
    if (!parts) {
        _engine.showToast("Invalid server: give host:port", engine::ToastType::Error);
        return;
    }
    std::optional<network::KeyFingerprint> pinned;
    try {
        pinned = network::KeyFingerprint::loadPublicKey(_publicKey);
    } catch (const std::exception &error) {
        logger().error("Cannot read the key of the server {}: {}", _publicKey.string(), error.what());
        _engine.showToast("server.pub is missing next to the client", engine::ToastType::Error);
        return;
    }
    // An IP is used as it is; a name is looked up, which blocks a moment, on a click
    _server = network::Address::parse(parts->first, parts->second);
    if (!_server) {
        try {
            _server = network::resolve(parts->first, parts->second).front();
        } catch (const std::exception &) {
            _engine.showToast(std::format("Unknown server {}", parts->first), engine::ToastType::Error);
            return;
        }
    }

    forgetLobby();
    _engine.showToast(std::format("Connecting to {}...", _serverText));
    _lobby = std::make_unique<LobbyClient>(
        _loop, *_server, *pinned, _name,
        LobbyClient::Handlers{
            .onWelcome = [this](std::uint32_t) { refresh(); },
            .onLeftGame =
                [this](const lobby::LeftGameMessage &left) {
                    _engine.showToast(leftReason(left.reason),
                                      left.reason == lobby::LeftGameReason::GameCrashed
                                          ? engine::ToastType::Error
                                          : engine::ToastType::Info);
                },
            .onClosed = [this](const std::string &reason) { lobbyClosed(reason); },
        });
}

void OnlineScenes::refresh()
{
    if (!_lobby) {
        return;
    }
    _lobby->listGames([this](const std::vector<lobby::GameInfo> &games) {
        _list = games;
        // Built again with the new list
        _engine.switchScene(LobbyScene);
    });
}

void OnlineScenes::joined(const LobbyClient::JoinResult &result)
{
    if (const auto *error = std::get_if<lobby::ErrorCode>(&result)) {
        _engine.showToast(describe(*error), engine::ToastType::Error);
        refresh();
        return;
    }
    const auto &game = std::get<lobby::GameJoinedMessage>(result);
    try {
        // The game is on the IP address of the master
        _games.play(std::make_unique<RemoteSession>(_loop, _server->withPort(game.udpPort), game),
                    [this] { gameEnded(); });
    } catch (const std::system_error &error) {
        _engine.showToast(std::format("Cannot join the game: {}", error.what()), engine::ToastType::Error);
        gameEnded();
    }
}

void OnlineScenes::gameEnded()
{
    if (!_lobby || !_lobby->welcomed()) {
        _engine.switchScene(MainMenuScene);
        return;
    }
    // Answered NOT_IN_GAME if the game already let the player go: back to the list all the same
    _lobby->leaveGame([this] { refresh(); });
}

void OnlineScenes::disconnect()
{
    if (_lobby) {
        _lobby->close();
    }
    forgetLobby();
    _engine.switchScene(MainMenuScene);
}

void OnlineScenes::lobbyClosed(const std::string &reason)
{
    _engine.showToast(reason, engine::ToastType::Error);
    forgetLobby();
    if (_engine.isSceneOpen(LobbyScene) || _engine.isSceneOpen(GameScene)) {
        _engine.switchScene(MainMenuScene);
    }
}

void OnlineScenes::forgetLobby()
{
    if (!_lobby) {
        return;
    }
    // Destroyed once the loop runs again: this may be one of its handlers
    std::shared_ptr<LobbyClient> old(std::move(_lobby));
    _loop.post([old] {});
}

} // namespace rtype::client
