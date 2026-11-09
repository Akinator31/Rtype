#pragma once

#include "Game/GameSessionSystem.hpp"
#include "ShipSelector/ShipSelection.hpp"
#include "rtype/discord/Presence.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Settings/Settings.hpp"
#include "rtype/localization/Localization.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

/// The activity of the player in its Discord profile (docs/architecture/client-online.md#rich-presence): what
/// the client is doing, read from its scenes and its game every frame, told to Discord when it changes.
namespace rtype::client
{

/// Where the choice "show my activity on Discord" of the Settings page is kept, on by default.
inline constexpr std::string_view DiscordActivitySettingsKey = "discord.show_activity";
/// The Discord application whose activity the client shows, its id in decimal: the official one of R-Type by
/// default (RTYPE_DISCORD_APPLICATION_ID of the build), 0 for none.
inline constexpr std::string_view DiscordApplicationSettingsKey = "discord.presence_application_id";

/// The application of the build, 0 when it has none.
[[nodiscard]] std::uint64_t defaultDiscordApplication();
/// The application kept in the settings, or the one of the build; 0 when it is not a number.
[[nodiscard]] std::uint64_t readDiscordApplication(const engine::Settings &settings);

/// What the player is doing, as Discord tells it.
struct PlayerActivity {
    enum class Place : std::uint8_t {
        /// The menus, the hangar, the settings, the lore.
        Hangar,
        /// The screens of a server: its games, its accounts.
        Online,
        /// A game in the client.
        Solo,
        /// A game on a server.
        OnlineGame,
        /// The results of a game online that went to its end.
        OnlineResults,
    };

    Place place = Place::Hangar;
    /// The last wave started, from 1; 0 before the first.
    std::uint16_t wave = 0;
    /// The waves of a game that has an end: solo.
    std::optional<std::uint16_t> waveCount;
    /// The players of a game online, and how many it takes.
    std::uint8_t players = 0;
    std::uint8_t maxPlayers = 0;
    /// When the game started.
    std::optional<std::chrono::system_clock::time_point> startedAt;
    /// The ship equipped: its code ("VX-03") and the key of its name.
    std::string shipCode;
    std::string shipNameKey;

    friend bool operator==(const PlayerActivity &, const PlayerActivity &) = default;
};

/// What the client is doing now: the game played, else the screens open.
[[nodiscard]] PlayerActivity observeActivity(const engine::Engine &engine, const GameSessionSystem &sessions,
                                             const ShipSelection &ships);

/// The activity of Discord of what the player is doing, in the language of the game:
///
/// | Place | Details | State | Party | Time |
/// |---|---|---|---|---|
/// | Hangar | "In the hangar" | | | |
/// | Online | "Online" | "Choosing a game" | | |
/// | Solo | "Solo" | "Wave 7/12" | | since the start |
/// | OnlineGame | "Online game" | "Wave 7" | 3/4 | since the start |
/// | OnlineResults | "Online game" | "Game over" | | |
///
/// No wave started, no state. The image is the ship equipped, its code and name over it. Nothing that tells
/// where the player is: neither the address of the server, nor the name of the game.
[[nodiscard]] discord::Activity describeActivity(const PlayerActivity &activity,
                                                 const localization::Localization &translations);

/// Keeps the activity of the player in Discord up to date: called every frame, it updates the presence when
/// the activity changes, at most once every MinInterval (Discord limits a presence to 5 updates in 20 s), the
/// last change sent once the interval passed. Off, it clears the presence and sends nothing.
///
/// @code
/// tracker.update(observeActivity(engine, sessions, ships), std::chrono::steady_clock::now());
/// @endcode
class PresenceTracker
{
public:
    using Clock = std::chrono::steady_clock;
    static constexpr std::chrono::seconds MinInterval{4};

    /// @param presence Outlives the tracker.
    /// @param translations The language of the texts, followed when it changes. Outlives the tracker.
    PresenceTracker(discord::Presence &presence, const localization::Localization &translations,
                    bool enabled = true);

    /// What the player is doing now.
    void update(const PlayerActivity &activity, Clock::time_point now);
    /// Shows the activity again from the next update, or clears it.
    void setEnabled(bool enabled);
    /// Sends the activity again at the next update, within the limit of Discord: once connected with a token,
    /// Discord cleared the one shown before.
    void resend() { _sent.reset(); }
    [[nodiscard]] bool isEnabled() const { return _enabled; }

private:
    discord::Presence &_presence;
    const localization::Localization &_translations;
    bool _enabled;
    /// The activity Discord was given last, none once cleared, and when.
    std::optional<discord::Activity> _sent;
    std::optional<Clock::time_point> _sentAt;
};

/// The choice of the Settings page kept in the settings: on when it is missing.
[[nodiscard]] bool readDiscordActivity(const engine::Settings &settings);

} // namespace rtype::client
