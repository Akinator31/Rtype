#include "Discord/DiscordPresence.hpp"

#include "Game/GameScenes.hpp"
#include "Game/RemoteSession.hpp"
#include "Lore/LoreScene.hpp"
#include "MainMenu/ControlsMenu.hpp"
#include "MainMenu/SettingsMenu.hpp"
#include "MainMenu/Submenus.hpp"
#include "MainMenu/UiShowcase.hpp"
#include "MainMenu/VoiceMenu.hpp"
#include "MainMenu/VrMenu.hpp"
#include "Online/OnlineScenes.hpp"
#include "Quests/QuestsMenu.hpp"
#include "ShipSelector/ShipSelector.hpp"
#include "rtype/lobby/Messages.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <initializer_list>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifndef RTYPE_DISCORD_APPLICATION_ID
#define RTYPE_DISCORD_APPLICATION_ID ""
#endif

namespace rtype::client
{

namespace
{

using Place = PlayerActivity::Place;

/// The screens open of each place out of a game, the first found wins: a page of the settings is open over
/// the main menu, the screens of a server over the menu Online.
struct ScenesOfPlace {
    Place place;
    std::vector<const char *> scenes;
};

const std::vector<ScenesOfPlace> &scenesOfPlaces()
{
    static const std::vector<ScenesOfPlace> places{
        {Place::Online, {LobbyScene}},
        {Place::Account, {MyAccountScene, DeleteAccountScene}},
        {Place::SigningIn,
         {AccountsScene, SignInScene, RegisterScene, CodeScene, ForgotPasswordScene, ResetPasswordScene,
          DiscordScene, ChooseUsernameScene}},
        {Place::Servers, {OnlineMenuScene, TrustServerScene, ChangedKeyScene}},
        {Place::Settings,
         {SettingsMenuScene, ControlsMenuScene, KeyCaptureScene, VoiceMenuScene, VoiceKeyCaptureScene,
          VrMenuScene, UiShowcaseScene}},
        {Place::Ships, {ShipSelectorScene}},
        {Place::Lore, {LoreScene}},
        {Place::Quests, {QuestsMenuScene}},
    };
    return places;
}

std::uint64_t parseApplication(std::string_view text)
{
    std::uint64_t id = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), id);
    return error == std::errc() && end == text.data() + text.size() ? id : 0;
}

/// "Wave 7/12", "Wave 7", or nothing before the first wave.
std::string waveText(const PlayerActivity &activity, const localization::Localization &translations)
{
    if (activity.wave == 0) {
        return {};
    }
    std::string text = translations.text("presence.wave") + " " + std::to_string(activity.wave);
    if (activity.waveCount && *activity.waveCount >= activity.wave) {
        text += "/" + std::to_string(*activity.waveCount);
    }
    return text;
}

/// What happens in the game: its end, else the final battle, else the pause, else a boss; nothing else.
std::string phaseText(const PlayerActivity &activity, const localization::Localization &translations)
{
    if (activity.outcome) {
        return translations.text(*activity.outcome == game::GameOutcome::Victory ? "presence.victory"
                                                                                 : "presence.defeat");
    }
    if (activity.finalBattle) {
        return translations.text("presence.final_battle");
    }
    if (activity.paused) {
        return translations.text("presence.paused");
    }
    if (activity.boss) {
        return translations.text("presence.boss");
    }
    return {};
}

/// The texts given, the empty ones left out, between dots: "Wave 7/12 · Boss fight".
std::string joined(std::initializer_list<std::string> parts)
{
    std::string text;
    for (const std::string &part : parts) {
        if (part.empty()) {
            continue;
        }
        if (!text.empty()) {
            text += " · ";
        }
        text += part;
    }
    return text;
}

/// "Wave 7/12 · Boss fight" in a game.
std::string gameText(const PlayerActivity &activity, const localization::Localization &translations)
{
    return joined({waveText(activity, translations), phaseText(activity, translations)});
}

/// "31 waves cleared", "1 wave cleared", or nothing.
std::string clearedText(std::uint16_t waves, const localization::Localization &translations)
{
    if (waves == 0) {
        return {};
    }
    return std::to_string(waves) + " " +
           translations.text(waves == 1 ? "presence.wave_cleared" : "presence.waves_cleared");
}

/// The key of the image of a ship uploaded in the application, its code in lower case: "vx-03".
std::string shipImage(const std::string &code)
{
    std::string image = code;
    std::ranges::transform(image, image.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return image;
}

} // namespace

std::uint64_t defaultDiscordApplication()
{
    return parseApplication(RTYPE_DISCORD_APPLICATION_ID);
}

std::uint64_t readDiscordApplication(const engine::Settings &settings)
{
    const std::optional<std::string> kept = settings.getString(DiscordApplicationSettingsKey);
    return kept ? parseApplication(*kept) : defaultDiscordApplication();
}

bool readDiscordActivity(const engine::Settings &settings)
{
    return settings.getBool(DiscordActivitySettingsKey).value_or(true);
}

PlayerActivity observeActivity(const engine::Engine &engine, const GameSessionSystem &sessions,
                               const ShipSelection &ships, const std::optional<GameResults> &results,
                               const std::optional<std::string> &lobbyServer)
{
    PlayerActivity activity;
    const ShipEntry &ship = getShipCatalog()[ships.equipped];
    activity.shipCode = ship.code;
    activity.shipNameKey = ship.nameKey;
    if (const GameSession *session = sessions.session()) {
        activity.place = session->isOnline() ? Place::OnlineGame : Place::Solo;
        activity.wave = sessions.wave();
        activity.waveCount = session->waveCount();
        activity.startedAt = sessions.startedAt();
        activity.boss = sessions.bossIsThere();
        activity.finalBattle = sessions.rescue() != game::RescueStage::None;
        activity.outcome = sessions.outcome();
        // Online, the game goes on behind the pause menu
        activity.paused = !session->isOnline() && engine.isSceneOpen(PauseScene);
        if (const auto *online = dynamic_cast<const RemoteSession *>(session)) {
            activity.players = static_cast<std::uint8_t>(sessions.players().size());
            activity.maxPlayers = lobby::MaxPlayers;
            activity.game = GameInvite{.server = online->server(), .gameId = online->gameId()};
        }
        return activity;
    }
    if (engine.isSceneOpen(ResultsScene)) {
        activity.place = Place::OnlineResults;
        activity.wavesCleared = results ? results->wavesCleared : 0;
        return activity;
    }
    for (const ScenesOfPlace &entry : scenesOfPlaces()) {
        if (std::ranges::any_of(entry.scenes,
                                [&engine](const char *scene) { return engine.isSceneOpen(scene); })) {
            activity.place = entry.place;
            if (entry.place == Place::Online && lobbyServer) {
                activity.game = GameInvite{.server = *lobbyServer};
                activity.players = 1;
                activity.maxPlayers = lobby::MaxPlayers;
            }
            return activity;
        }
    }
    activity.place = Place::Hangar;
    return activity;
}

discord::Activity describeActivity(const PlayerActivity &activity,
                                   const localization::Localization &translations)
{
    discord::Activity result;
    switch (activity.place) {
    case Place::Hangar:
        result.details = translations.text("presence.hangar");
        break;
    case Place::Ships:
        result.details = translations.text("presence.hangar");
        result.state = translations.text("presence.choosing_ship");
        break;
    case Place::Settings:
        result.details = translations.text("presence.settings");
        break;
    case Place::Lore:
        result.details = translations.text("presence.hangar");
        result.state = translations.text("presence.lore");
        break;
    case Place::Quests:
        result.details = translations.text("presence.hangar");
        result.state = translations.text("presence.quests");
        break;
    case Place::Servers:
        result.details = translations.text("presence.online");
        result.state = translations.text("presence.choosing_server");
        break;
    case Place::SigningIn:
        result.details = translations.text("presence.online");
        result.state = translations.text("presence.signing_in");
        break;
    case Place::Account:
        result.details = translations.text("presence.online");
        result.state = translations.text("presence.account");
        break;
    case Place::Online:
        result.details = translations.text("presence.online");
        result.state = translations.text("presence.choosing_game");
        // Friends invited from Discord come to the server: a party of one, with room
        if (activity.game && activity.maxPlayers > 1) {
            result.party =
                discord::Party{.id = partyId(*activity.game), .size = 1, .max = activity.maxPlayers};
            result.joinSecret = inviteSecret(*activity.game).value_or(std::string());
        }
        break;
    case Place::Solo:
        result.details = translations.text("presence.solo");
        result.state = gameText(activity, translations);
        result.startedAt = activity.startedAt;
        break;
    case Place::OnlineGame:
        result.details = translations.text("presence.online_game");
        result.state = gameText(activity, translations);
        result.startedAt = activity.startedAt;
        if (activity.maxPlayers > 0) {
            const std::uint8_t size = std::max<std::uint8_t>(activity.players, 1);
            result.party = discord::Party{.id = activity.game ? partyId(*activity.game) : std::string(),
                                          .size = size,
                                          .max = activity.maxPlayers};
            // Joinable while there is room, and the game is not over
            if (activity.game && size < activity.maxPlayers && !activity.outcome) {
                result.joinSecret = inviteSecret(*activity.game).value_or(std::string());
            }
        }
        break;
    case Place::OnlineResults:
        result.details = translations.text("presence.online_game");
        result.state = joined(
            {translations.text("presence.game_over"), clearedText(activity.wavesCleared, translations)});
        break;
    }
    result.largeImage = std::string(PresenceLogoImage);
    result.largeText = "R-Type";
    // The ship flown, in a game only
    const bool inGame = activity.place == Place::Solo || activity.place == Place::OnlineGame;
    if (inGame && !activity.shipCode.empty()) {
        result.smallImage = shipImage(activity.shipCode);
        result.smallText = activity.shipCode;
        if (!activity.shipNameKey.empty()) {
            result.smallText += " · " + translations.text(activity.shipNameKey);
        }
    }
    return result;
}

PresenceTracker::PresenceTracker(discord::Presence &presence, const localization::Localization &translations,
                                 bool enabled)
    : _presence(presence), _translations(translations), _enabled(enabled)
{
}

void PresenceTracker::update(const PlayerActivity &activity, Clock::time_point now)
{
    if (!_enabled) {
        return;
    }
    if (std::exchange(_answer->refused, false)) {
        // Discord did not take it: sent again later, whatever the player does by then
        _sent.reset();
        _retryAt = now + RetryInterval;
    }
    discord::Activity described = describeActivity(activity, _translations);
    if (_sent == described) {
        return;
    }
    if ((_sentAt && now - *_sentAt < MinInterval) || (_retryAt && now < *_retryAt)) {
        // Sent once the interval passed, if it is still what the player does
        return;
    }
    const std::uint64_t attempt = ++_answer->attempt;
    _presence.update(described, [answer = std::weak_ptr<Answer>(_answer), attempt](bool shown) {
        const std::shared_ptr<Answer> alive = answer.lock();
        if (alive && alive->attempt == attempt && !shown) {
            alive->refused = true;
        }
    });
    _sent = std::move(described);
    _sentAt = now;
    _retryAt.reset();
}

void PresenceTracker::resend()
{
    // At the next frame, whatever the interval: Discord shows nothing meanwhile
    _sent.reset();
    _sentAt.reset();
    _retryAt.reset();
    // What Discord answers for the activity it lost no longer counts
    ++_answer->attempt;
    _answer->refused = false;
}

void PresenceTracker::setEnabled(bool enabled)
{
    if (enabled == _enabled) {
        return;
    }
    _enabled = enabled;
    if (!enabled && _sent) {
        _presence.clear();
    }
    // A clear counts as an update for the limit of Discord: _sentAt is kept
    _sent.reset();
    _retryAt.reset();
    ++_answer->attempt;
    _answer->refused = false;
}

} // namespace rtype::client
