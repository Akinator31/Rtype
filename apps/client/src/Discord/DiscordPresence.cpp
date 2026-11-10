#include "Discord/DiscordPresence.hpp"

#include "Game/GameScenes.hpp"
#include "MainMenu/Submenus.hpp"
#include "Online/OnlineScenes.hpp"
#include "rtype/lobby/Messages.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <string>
#include <system_error>

#ifndef RTYPE_DISCORD_APPLICATION_ID
#define RTYPE_DISCORD_APPLICATION_ID ""
#endif

namespace rtype::client
{

namespace
{

/// The screens of a server, over which the player is "online".
constexpr std::array OnlineScenesShown{
    OnlineMenuScene, LobbyScene,         TrustServerScene, ChangedKeyScene,     AccountsScene,
    SignInScene,     RegisterScene,      CodeScene,        ForgotPasswordScene, ResetPasswordScene,
    MyAccountScene,  DeleteAccountScene, DiscordScene,     ChooseUsernameScene,
};

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
                               const ShipSelection &ships)
{
    PlayerActivity activity;
    const ShipEntry &ship = getShipCatalog()[ships.equipped];
    activity.shipCode = ship.code;
    activity.shipNameKey = ship.nameKey;
    if (const GameSession *session = sessions.session()) {
        activity.place =
            session->isOnline() ? PlayerActivity::Place::OnlineGame : PlayerActivity::Place::Solo;
        activity.wave = sessions.wave();
        activity.waveCount = session->waveCount();
        activity.startedAt = sessions.startedAt();
        if (session->isOnline()) {
            activity.players = static_cast<std::uint8_t>(sessions.players().size());
            activity.maxPlayers = lobby::MaxPlayers;
        }
        return activity;
    }
    if (engine.isSceneOpen(ResultsScene)) {
        activity.place = PlayerActivity::Place::OnlineResults;
        return activity;
    }
    const bool online = std::ranges::any_of(
        OnlineScenesShown, [&engine](const char *scene) { return engine.isSceneOpen(scene); });
    activity.place = online ? PlayerActivity::Place::Online : PlayerActivity::Place::Hangar;
    return activity;
}

discord::Activity describeActivity(const PlayerActivity &activity,
                                   const localization::Localization &translations)
{
    discord::Activity result;
    switch (activity.place) {
    case PlayerActivity::Place::Hangar:
        result.details = translations.text("presence.hangar");
        break;
    case PlayerActivity::Place::Online:
        result.details = translations.text("presence.online");
        result.state = translations.text("presence.choosing_game");
        break;
    case PlayerActivity::Place::Solo:
        result.details = translations.text("presence.solo");
        result.state = waveText(activity, translations);
        result.startedAt = activity.startedAt;
        break;
    case PlayerActivity::Place::OnlineGame:
        result.details = translations.text("presence.online_game");
        result.state = waveText(activity, translations);
        result.startedAt = activity.startedAt;
        if (activity.maxPlayers > 0) {
            result.party = discord::Party{.size = std::max<std::uint8_t>(activity.players, 1),
                                          .max = activity.maxPlayers};
        }
        break;
    case PlayerActivity::Place::OnlineResults:
        result.details = translations.text("presence.online_game");
        result.state = translations.text("presence.game_over");
        break;
    }
    if (!activity.shipCode.empty()) {
        result.largeImage = shipImage(activity.shipCode);
        result.largeText = activity.shipCode;
        if (!activity.shipNameKey.empty()) {
            result.largeText += " · " + translations.text(activity.shipNameKey);
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
    discord::Activity described = describeActivity(activity, _translations);
    if (_sent == described) {
        return;
    }
    if (_sentAt && now - *_sentAt < MinInterval) {
        // Sent once the interval passed, if it is still what the player does
        return;
    }
    _presence.update(described);
    _sent = std::move(described);
    _sentAt = now;
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
}

} // namespace rtype::client
