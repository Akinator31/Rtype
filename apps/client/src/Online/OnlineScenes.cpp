#include "Online/OnlineScenes.hpp"

#include "Assets.hpp"
#include "Game/RemoteSession.hpp"
#include "MainMenu/Layout.hpp"
#include "MainMenu/MainMenu.hpp"
#include "MainMenu/Submenus.hpp"
#include "ShipSelector/ShipSelector.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/game/Progression.hpp"
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
constexpr std::size_t MaxGamesShown = 5;
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
    case lobby::LeftGameReason::Kicked:
        return "An administrator removed you from the game";
    case lobby::LeftGameReason::EndedByAdmin:
        return "An administrator ended the game";
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

std::vector<std::string> fingerprintLines(const network::KeyFingerprint &fingerprint,
                                          std::size_t groupsPerLine)
{
    constexpr std::size_t GroupSize = 4;
    const std::string hex = fingerprint.toHex();
    const std::size_t perLine = std::max<std::size_t>(groupsPerLine, 1) * GroupSize;
    std::vector<std::string> lines;
    for (std::size_t line = 0; line < hex.size(); line += perLine) {
        std::string text;
        for (std::size_t group = line; group < std::min(line + perLine, hex.size()); group += GroupSize) {
            if (!text.empty()) {
                text += ' ';
            }
            text += hex.substr(group, GroupSize);
        }
        lines.push_back(std::move(text));
    }
    return lines;
}

OnlineScenes::OnlineScenes(engine::Engine &engine, render::ResourceManager &resources,
                           network::EventLoop &loop, GameScenes &games, const engine::InterfaceStyle &style,
                           localization::Localization &translations, discord::Authorizer &discord,
                           discord::Connection &discordConnection, ShipSelection &ships,
                           std::filesystem::path pictures, const game::GameConfig &config,
                           VoiceDevices &voice)
    : _engine(engine), _style(style), _resources(resources), _loop(loop), _discord(discord),
      _discordConnection(discordConnection), _games(games), _translations(translations), _ships(ships),
      _config(config), _voice(voice), _known(engine.getSettings()), _sessions(engine.getSettings()),
      _profiles(engine.getSettings(), std::move(pictures)),
      _sky(resources.loadTexture(RTYPE_IMAGE_STARFIELD, render::TextureFiltering::Smooth)),
      _discordLogo(resources.loadTexture(RTYPE_IMAGE_DISCORD, render::TextureFiltering::Smooth))
{
    _discordConnection.onTokenExpiring([this] { refreshDiscord(); });
    declareServerScenes(style);
    declareLobbyScene(style);
    declareAccountScenes(style);
}

OnlineScreen OnlineScenes::screen(engine::Engine &target, engine::SceneId scene, bool sky) const
{
    if (sky) {
        const ecs::entity_t backdrop = target.getEcs().createEntity();
        target.getEcs().addComponent<engine::SceneMember>(backdrop, scene);
        target.getEcs().addComponent<engine::Backdrop>(backdrop, _sky);
    }
    return OnlineScreen(target, scene, _style);
}

void OnlineScenes::declareServerScenes(const engine::InterfaceStyle &style)
{
    using online_layout::columnLeft;
    using online_layout::rightLeft;
    using online_layout::rightWidth;
    using Tone = OnlineScreen::Tone;

    // O1: the name and the server on the left, the servers met on the right
    _engine.createScene(
        OnlineMenuScene, {.overlay = true, .hidesInterfaceBelow = true},
        [this, &style](engine::Engine &target, engine::SceneId scene) {
            OnlineScreen page(target, scene, style);
            page.header(_translations.text("menu.online"), [&target] { target.popScene(); });
            const ecs::entity_t name =
                page.field(_translations.text("online.name"), _name, 0, lobby::MaxPlayerNameLength);
            const auto read = [this, &target, name](ecs::entity_t server) {
                _name = target.getEcs().getComponent<engine::TextField>(name).input.text();
                _serverText = target.getEcs().getComponent<engine::TextField>(server).input.text();
            };
            const auto server = std::make_shared<ecs::entity_t>(0);
            *server = page.field(_translations.text("online.server"), _serverText, 1, 64,
                                 [this, read, server](const std::string &) {
                                     read(*server);
                                     connect();
                                 });
            page.note(_translations.text("online.server_hint"),
                      {engine::AnchorPoint::TopLeft, columnLeft, online_layout::fieldsTop + 250.0f},
                      Tone::Hint, 20.0f);
            page.button(_translations.text("online.connect"),
                        [this, read, server] {
                            read(*server);
                            connect();
                        },
                        {engine::AnchorPoint::TopLeft, columnLeft, 600.0f});
            page.button(_translations.text("online.forget"),
                        [this, read, server] {
                            read(*server);
                            forgetServer();
                        },
                        {engine::AnchorPoint::TopLeft, columnLeft, 600.0f + online_layout::buttonsSpacing});

            // The servers met: those whose key the player trusted, with the account remembered for each
            page.note(toCapitals(_translations.text("online.known")),
                      {engine::AnchorPoint::TopLeft, rightLeft, online_layout::fieldsTop - 14.0f},
                      Tone::Detail, 20.0f);
            const std::vector<std::string> keys = target.getSettings().keys(KnownServers::SettingsKeyPrefix);
            if (keys.empty()) {
                page.note(_translations.text("online.known_none"),
                          {engine::AnchorPoint::TopLeft, rightLeft, online_layout::fieldsTop + 30.0f},
                          Tone::Hint);
            }
            constexpr std::size_t MaxServersShown = 5;
            float top = online_layout::fieldsTop + 30.0f;
            for (std::size_t index = 0; index < std::min(keys.size(), MaxServersShown); ++index) {
                const std::string address = keys[index].substr(KnownServers::SettingsKeyPrefix.size());
                const std::optional<RememberedProfile> account = _profiles.profile(address);
                engine::Avatar avatar{.initial = "?", .kind = engine::AvatarKind::Guest};
                std::string detail = _translations.text("online.known_guest");
                if (account) {
                    avatar.kind =
                        account->discord ? engine::AvatarKind::Discord : engine::AvatarKind::Account;
                    avatar.initial = initialOf(account->username);
                    if (const auto png = _profiles.picture(account->avatarHash)) {
                        avatar.picture = roundPicture(account->avatarHash, std::as_bytes(std::span(*png)));
                    }
                    detail = std::format("{} · {}", account->username,
                                         _translations.text(account->discord ? "online.known_discord"
                                                                             : "online.known_account"));
                }
                page.ui().listRow(toCapitals(address),
                                  {.detail = detail + " · " + _translations.text("online.known_trusted"),
                                   .width = rightWidth,
                                   .avatar = avatar},
                                  [this, address] {
                                      _serverText = address;
                                      connect();
                                  },
                                  {engine::AnchorPoint::TopLeft, rightLeft, top});
                top += style.list.height;
            }
            page.keys(_translations.text("online.keys_menu"));
        });

    // O2: a server met for the first time shows its key, and is trusted only if the player says so
    _engine.createScene(TrustServerScene, {.overlay = true, .hidesInterfaceBelow = true},
                        [this, &style](engine::Engine &target, engine::SceneId scene) {
                            OnlineScreen page(target, scene, style);
                            page.veil();
                            page.question(_translations.text("trust.title"), 236.0f);
                            page.rule({engine::AnchorPoint::Top, 0.0f, 318.0f}, 720.0f, {255, 255, 255, 120});
                            const ecs::entity_t first =
                                page.note(std::format("{} {}.", _translations.text("trust.first"), _address),
                                          {engine::AnchorPoint::Top, 0.0f, 352.0f}, Tone::Detail, 24.0f);
                            (void)first;
                            page.note(_translations.text("trust.check"),
                                      {engine::AnchorPoint::Top, 0.0f, 390.0f}, Tone::Detail, 24.0f);
                            if (_presented) {
                                float top = 470.0f;
                                for (const std::string &line : fingerprintLines(*_presented)) {
                                    const ecs::entity_t text = page.note(
                                        line, {engine::AnchorPoint::Top, 0.0f, top}, Tone::Bright, 30.0f);
                                    target.getEcs().getComponent<engine::Text>(text).font =
                                        style.field.valueFont;
                                    top += 56.0f;
                                }
                            }
                            // Not trusting is the default: a key is trusted on purpose
                            page.button(_translations.text("trust.cancel"), [&target] { target.popScene(); },
                                        {engine::AnchorPoint::Top, -330.0f, 680.0f});
                            page.button(_translations.text("trust.accept"),
                                        [this, &target] {
                                            target.popScene();
                                            if (_presented && !_known.trust(_address, *_presented)) {
                                                _engine.showToast(_translations.text("trust.not_kept"),
                                                                  engine::ToastType::Error);
                                            }
                                            connect();
                                        },
                                        {engine::AnchorPoint::Top, 330.0f, 680.0f});
                            page.note(toCapitals(_translations.text("online.keys_cancel")),
                                      {engine::AnchorPoint::Bottom, 0.0f, -84.0f}, Tone::Hint, 20.0f);
                        });

    // O3: a known server with another key is refused, the two keys side by side, under a red banner
    _engine.createScene(
        ChangedKeyScene, {.overlay = true, .hidesInterfaceBelow = true},
        [this, &style](engine::Engine &target, engine::SceneId scene) {
            OnlineScreen page(target, scene, style);
            page.veil();
            constexpr render::Color red{232, 114, 110, 255};
            constexpr float bandTop = 184.0f;
            constexpr float bandHeight = 200.0f;
            page.ui().box(8000.0f, bandHeight, {232, 114, 110, 18},
                          {engine::AnchorPoint::Top, 0.0f, bandTop});
            page.rule({engine::AnchorPoint::Top, 0.0f, bandTop}, 1920.0f, {232, 114, 110, 200});
            page.rule({engine::AnchorPoint::Top, 0.0f, bandTop + bandHeight}, 1920.0f, {232, 114, 110, 200});
            const ecs::entity_t title =
                page.note(toCapitals(_translations.text("changed.title")),
                          {engine::AnchorPoint::Top, 0.0f, bandTop + 48.0f}, Tone::Alert, 52.0f);
            target.getEcs().getComponent<engine::Text>(title).font = style.field.valueFont;
            target.getEcs().getComponent<engine::Text>(title).color = red;
            page.note(toCapitals(std::format("{} · {}", _address, _translations.text("changed.refused"))),
                      {engine::AnchorPoint::Top, 0.0f, bandTop + 128.0f}, Tone::Detail, 20.0f);
            const auto keyColumn = [&](const std::string &caption, const network::KeyFingerprint &key,
                                       float x, Tone tone) {
                page.note(toCapitals(caption), {engine::AnchorPoint::Top, x, 450.0f}, tone, 20.0f);
                float top = 492.0f;
                for (const std::string &line : fingerprintLines(key, 4)) {
                    const ecs::entity_t text =
                        page.note(line, {engine::AnchorPoint::Top, x, top}, tone, 24.0f);
                    target.getEcs().getComponent<engine::Text>(text).font = style.field.valueFont;
                    top += 40.0f;
                }
            };
            const std::vector<network::KeyFingerprint> expected = _known.trusted(_address);
            if (!expected.empty()) {
                keyColumn(_translations.text("changed.expected"), expected.front(), -300.0f, Tone::Detail);
            }
            if (_presented) {
                keyColumn(_translations.text("changed.presented"), *_presented, 300.0f, Tone::Alert);
            }
            page.note(_translations.text("changed.attack"), {engine::AnchorPoint::Top, 0.0f, 700.0f},
                      Tone::Detail, 24.0f);
            page.note(_translations.text("changed.forget"), {engine::AnchorPoint::Top, 0.0f, 738.0f},
                      Tone::Detail, 24.0f);
            page.button(_translations.text("changed.back"), [&target] { target.popScene(); },
                        {engine::AnchorPoint::Top, 0.0f, 820.0f});
        });
}

void OnlineScenes::declareLobbyScene(const engine::InterfaceStyle &style)
{
    using online_layout::columnLeft;
    using Tone = OnlineScreen::Tone;
    constexpr float listWidth = 1120.0f;
    constexpr float listTop = 280.0f;

    // L1: the games of the server in a list, the pilot at the top right, its avatar opening My account
    _engine.createScene(
        LobbyScene, {.onBack = [this] { disconnect(); }},
        [this, &style](engine::Engine &target, engine::SceneId scene) {
            OnlineScreen page = screen(target, scene);
            page.header(_serverText, [this] { disconnect(); });
            page.note(toCapitals(_translations.text("online.games")),
                      {engine::AnchorPoint::TopLeft, columnLeft, listTop - 44.0f}, Tone::Detail, 20.0f);
            const ecs::entity_t count = page.note(
                toCapitals(std::format("{} {}", _list.size(), _translations.text("online.games_count"))),
                {engine::AnchorPoint::TopLeft, columnLeft, listTop - 44.0f}, Tone::Detail, 20.0f);
            target.getEcs().getComponent<engine::Anchor>(count) = {
                engine::AnchorPoint::TopRight, -(1920.0f - columnLeft - listWidth), listTop - 44.0f};
            float top = listTop;
            for (std::size_t i = 0; i < std::min(_list.size(), MaxGamesShown); ++i) {
                const lobby::GameInfo &game = _list[i];
                const bool full = game.players >= game.maxPlayers;
                const std::string players = std::format("{} / {} {}", game.players, game.maxPlayers,
                                                        _translations.text("online.pilots"));
                std::function<void()> join;
                if (!full) {
                    join = [this, id = game.id] {
                        openShipSelector(_engine, _ships, [this, id] {
                            if (_lobby) {
                                _lobby->joinGame(
                                    id, [this](const LobbyClient::JoinResult &result) { joined(result); });
                            }
                        });
                    };
                }
                page.ui().listRow(
                    toCapitals(game.name),
                    {.detail = players,
                     .side = toCapitals(_translations.text(full ? "online.full" : "online.join")),
                     .width = listWidth},
                    std::move(join), {engine::AnchorPoint::TopLeft, columnLeft, top});
                top += style.list.height;
            }
            if (_list.empty()) {
                page.note(_translations.text("online.empty"),
                          {engine::AnchorPoint::TopLeft, columnLeft, listTop + 20.0f}, Tone::Hint);
            }
            const float buttons = listTop + (static_cast<float>(MaxGamesShown) * style.list.height) + 30.0f;
            page.button(_translations.text("online.create"),
                        [this] {
                            openShipSelector(_engine, _ships, [this] {
                                if (_lobby) {
                                    const std::string game = std::string(_lobby->name())
                                                                 .append("'s game")
                                                                 .substr(0, lobby::MaxGameNameLength);
                                    _lobby->createGame(
                                        game, CreatedGamePlayers,
                                        [this](const LobbyClient::JoinResult &result) { joined(result); });
                                }
                            });
                        },
                        {engine::AnchorPoint::TopLeft, columnLeft, buttons});
            page.button(_translations.text("online.refresh"), [this] { refresh(); },
                        {engine::AnchorPoint::TopLeft, columnLeft, buttons + online_layout::buttonsSpacing});

            // The pilot, at the top right, its avatar on the right: My account for an account, signing in for
            // a guest who can
            constexpr float avatarSize = 72.0f;
            const bool signedIn = _lobby && _lobby->account();
            const bool canSignIn =
                _lobby && (_lobby->features().passwordAccounts || _lobby->features().discord);
            page.ui().avatar(accountAvatar(avatarSize),
                             {engine::AnchorPoint::TopRight, -screenMargin, screenMargin - 8.0f});
            const float nameRight = -screenMargin - avatarSize - 12.0f;
            page.button(toCapitals(_lobby ? _lobby->name() : _name),
                        [this, signedIn, canSignIn] {
                            if (signedIn) {
                                _engine.switchScene(MyAccountScene);
                            } else if (canSignIn) {
                                _engine.switchScene(AccountsScene);
                            }
                        },
                        {engine::AnchorPoint::TopRight, nameRight, screenMargin - 18.0f});
            std::string under = toCapitals(_translations.text(signedIn ? "account.mine" : "online.guest"));
            if (!signedIn && canSignIn) {
                under += " · " + toCapitals(_translations.text("account.sign_in"));
            }
            if (signedIn && _lobby->progress()) {
                under += std::format(" · {} {}", _translations.text("progress.level"),
                                     game::levelOf(_lobby->progress()->xp).level);
            }
            page.note(under, {engine::AnchorPoint::TopRight, nameRight - 12.0f, screenMargin + 40.0f},
                      Tone::Hint, 20.0f);
            page.keys(_translations.text("online.keys_lobby"));
        });
}

void OnlineScenes::connect()
{
    const auto parts = splitServer(_serverText);
    if (!parts) {
        _engine.showToast("Invalid server: give host:port", engine::ToastType::Error);
        return;
    }
    // With the token of an account, the name of the player is its username
    const std::optional<lobby::SessionToken> token =
        _sessions.token(serverAddress(parts->first, parts->second));
    if (!token && !lobby::isValidPlayerName(_name)) {
        _engine.showToast(message(lobby::ErrorCode::InvalidName), engine::ToastType::Error);
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

    _address = serverAddress(parts->first, parts->second);

    forgetLobby();
    _profile.reset();
    _picture.reset();
    _newUser.reset();
    _newUserPicture.reset();
    _engine.showToast(std::format("Connecting to {}...", _serverText));
    _lobby = std::make_unique<LobbyClient>(
        _loop, *_server, _known.trusted(_address), _name,
        LobbyClient::Handlers{
            .onWelcome = [this](std::uint32_t) { welcomed(); },
            .onLeftGame =
                [this](const lobby::LeftGameMessage &left) {
                    // The results of the game tell it already
                    if (left.reason == lobby::LeftGameReason::GameOver && _games.results()) {
                        return;
                    }
                    _engine.showToast(leftReason(left.reason),
                                      left.reason == lobby::LeftGameReason::GameCrashed ||
                                              left.reason == lobby::LeftGameReason::Kicked
                                          ? engine::ToastType::Error
                                          : engine::ToastType::Info);
                },
            .onServerMessage =
                [this](const std::string &text) { _engine.showToast(std::format("Server: {}", text)); },
            .onProgress = [this](const lobby::ProgressMessage &progress) { showProgress(progress); },
            .onDiscordToken = [this](const lobby::DiscordTokenMessage &token) { discordToken(token); },
            .onClosed = [this](const std::string &reason) { lobbyClosed(reason); },
            .onUntrusted = [this](const network::KeyFingerprint &presented) { untrusted(presented); },
            .onSessionExpired =
                [this] {
                    // The token is no longer good: forgotten, and the player comes back as a guest
                    _sessions.forget(_address);
                    _engine.showToast(message(lobby::ErrorCode::SessionExpired), engine::ToastType::Warning);
                    connect();
                },
            .describe = [this](lobby::ErrorCode code) { return message(code); },
        },
        _translations.language(), token);
}

void OnlineScenes::untrusted(const network::KeyFingerprint &presented)
{
    forgetLobby();
    _presented = presented;
    switch (_known.judge(_address)) {
    case KnownServers::Verdict::FirstUse:
        _engine.pushScene(TrustServerScene);
        return;
    case KnownServers::Verdict::Changed:
        logger().error("{} presented the key {}, not the one trusted: refused", _address, presented.toHex());
        _engine.pushScene(ChangedKeyScene);
        return;
    }
}

void OnlineScenes::forgetServer()
{
    const auto parts = splitServer(_serverText);
    if (!parts) {
        _engine.showToast("Invalid server: give host:port", engine::ToastType::Error);
        return;
    }
    const std::string address = serverAddress(parts->first, parts->second);
    if (_known.forget(address)) {
        _engine.showToast(
            std::format("Key of {} forgotten: it is asked again at the next connection", address));
    } else {
        _engine.showToast(std::format("No key trusted for {}", address));
    }
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
        _engine.showToast(message(*error), engine::ToastType::Error);
        refresh();
        return;
    }
    const auto &game = std::get<lobby::GameJoinedMessage>(result);
    // The experience before the game, from which its results fill the bar of the level
    _xpBefore.reset();
    if (_lobby && _lobby->account()) {
        _xpBefore = _lobby->progress() ? _lobby->progress()->xp : 0;
    }
    try {
        // The game is on the IP address of the master
        _games.play(std::make_unique<RemoteSession>(_loop, _server->withPort(game.udpPort), game, _config,
                                                    &_voice, _address),
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
    if (_games.results()) {
        // The game went to its end: its results, then back to the list once the player continues
        _lobby->leaveGame();
        _games.showResults(_xpBefore, _lobby->features().passwordAccounts || _lobby->features().discord,
                           [this] {
                               if (_lobby) {
                                   refresh();
                               } else {
                                   _engine.switchScene(MainMenuScene);
                               }
                           });
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

void OnlineScenes::showProgress(const lobby::ProgressMessage &progress)
{
    refreshIfShown(LobbyScene);
    // At the sign-in, the level shows in the menu: only a game is announced
    if (progress.gained == 0) {
        return;
    }
    // The game just played, or its results, show it; in a toast otherwise
    _games.setAccountXp(progress.xp);
    if (_engine.isSceneOpen(GameScene) || _engine.isSceneOpen(ResultsScene)) {
        return;
    }
    const game::Level level = game::levelOf(progress.xp);
    const game::Level before =
        game::levelOf(progress.xp - std::min<std::uint64_t>(progress.gained, progress.xp));
    const std::string title = std::format("{} {}", _translations.text("progress.level"), level.level);
    const std::string detail =
        std::format("+{} {} - {} / {} XP", progress.gained, _translations.text("progress.gained"),
                    level.xpIntoLevel, level.xpForNext);
    if (level.level > before.level) {
        _engine.showToast(_translations.text("progress.level_up"), title, "", engine::ToastType::Success);
    }
    _engine.showToast(title, detail, "", engine::ToastType::Info);
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
    // What Discord would give goes to no connection, and the Discord of this server is left
    _discord.cancel();
    _discordConnection.disconnect();
    // Destroyed once the loop runs again: this may be one of its handlers
    std::shared_ptr<LobbyClient> old(std::move(_lobby));
    _loop.post([old] {});
}

void OnlineScenes::readProfile()
{
    if (!_lobby || !_lobby->account()) {
        return;
    }
    const lobby::AccountInfo account = *_lobby->account();
    _lobby->profile(account.id, [this, account](const auto &result) {
        if (!_lobby) {
            return;
        }
        if (const auto *error = std::get_if<lobby::ErrorCode>(&result)) {
            logger().warning("No profile for account {}: {}", account.id, describe(*error));
            return;
        }
        _profile = std::get<lobby::ProfileMessage>(result);
        _profiles.remember(
            _address,
            {.username = account.username, .discord = _profile->discord, .avatarHash = _profile->avatarHash});
        refreshIfShown(LobbyScene);
        refreshIfShown(MyAccountScene);
        if (_profile->avatarHash.empty()) {
            return;
        }
        // A picture kept already is not asked again
        const std::string hash = _profile->avatarHash;
        if (const auto png = _profiles.picture(hash)) {
            _picture = roundPicture(hash, std::as_bytes(std::span(*png)));
            refreshIfShown(LobbyScene);
            refreshIfShown(MyAccountScene);
            return;
        }
        _lobby->picture(account.id, [this, hash](const auto &picture) {
            if (const auto *received = std::get_if<LobbyClient::Picture>(&picture)) {
                _profiles.keepPicture(received->hash, received->png);
                _picture = roundPicture(received->hash, received->png);
                refreshIfShown(LobbyScene);
                refreshIfShown(MyAccountScene);
            }
        });
    });
}

std::optional<render::TextureId> OnlineScenes::roundPicture(const std::string &hash,
                                                            std::span<const std::byte> png)
{
    constexpr int PictureSize = 128;
    try {
        const auto *first = reinterpret_cast<const unsigned char *>(png.data());
        return _resources.loadRoundPicture(hash, std::vector<unsigned char>(first, first + png.size()),
                                           PictureSize);
    } catch (const std::exception &error) {
        logger().warning("Cannot show the picture {}: {}", hash, error.what());
        return std::nullopt;
    }
}

engine::Avatar OnlineScenes::accountAvatar(float size) const
{
    if (!_lobby || !_lobby->account()) {
        return {.initial = "?", .kind = engine::AvatarKind::Guest, .size = size};
    }
    return {.picture = _picture,
            .initial = initialOf(_lobby->account()->username),
            .kind = _profile && _profile->discord ? engine::AvatarKind::Discord : engine::AvatarKind::Account,
            .size = size};
}

void OnlineScenes::refreshIfShown(const char *scene)
{
    if (_engine.isSceneOpen(scene)) {
        _engine.switchScene(scene);
    }
}

} // namespace rtype::client
