#pragma once

#include "Discord/DiscordInvite.hpp"
#include "Game/GameSessionSystem.hpp"
#include "Game/ResultsScreen.hpp"
#include "ShipSelector/ShipSelection.hpp"
#include "rtype/discord/Presence.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Settings/Settings.hpp"
#include "rtype/game/Snapshot.hpp"
#include "rtype/localization/Localization.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
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

/// The key of the logo of the game on the Rich Presence page of the application: the large image of the
/// activity.
inline constexpr std::string_view PresenceLogoImage = "logo";

/// The application of the build, 0 when it has none.
[[nodiscard]] std::uint64_t defaultDiscordApplication();
/// The application kept in the settings, or the one of the build; 0 when it is not a number.
[[nodiscard]] std::uint64_t readDiscordApplication(const engine::Settings &settings);

/// What the player is doing, as Discord tells it.
struct PlayerActivity {
    enum class Place : std::uint8_t {
        /// The main menu, the menu Play.
        Hangar,
        /// The ship selector.
        Ships,
        /// The settings and their pages: the controls, the voice, the VR.
        Settings,
        /// The lore of the game.
        Lore,
        /// The quests.
        Quests,
        /// The servers known, and the trust of a server.
        Servers,
        /// Signing in on a server: its accounts, the password, the code of the email, Discord.
        SigningIn,
        /// The account signed in on a server.
        Account,
        /// The games of a server.
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
    /// A boss is in the game.
    bool boss = false;
    /// The rescue against the final boss runs (solo).
    bool finalBattle = false;
    /// The game is stopped by the pause menu (solo: online, the game goes on).
    bool paused = false;
    /// How the game ended, once it did.
    std::optional<game::GameOutcome> outcome;
    /// The waves the player cleared in the game online whose results are shown.
    std::uint16_t wavesCleared = 0;
    /// The players of a game online, and how many it takes.
    std::uint8_t players = 0;
    std::uint8_t maxPlayers = 0;
    /// When the game started.
    std::optional<std::chrono::system_clock::time_point> startedAt;
    /// The game online, which friends can be invited to: none offline.
    std::optional<GameInvite> game;
    /// The ship equipped: its code ("VX-03") and the key of its name.
    std::string shipCode;
    std::string shipNameKey;

    friend bool operator==(const PlayerActivity &, const PlayerActivity &) = default;
};

/// What the client is doing now: the game played, else the results of the last game online, else the screens
/// open. A game online is told with its server and its id (RemoteSession), for the invitations.
/// @param results What the player did in the last game online that went to its end (GameScenes::results()).
/// @param lobbyServer The server whose list of games the player looks at (OnlineScenes::lobbyServer()):
/// friends
///   are invited to it.
[[nodiscard]] PlayerActivity observeActivity(const engine::Engine &engine, const GameSessionSystem &sessions,
                                             const ShipSelection &ships,
                                             const std::optional<GameResults> &results = std::nullopt,
                                             const std::optional<std::string> &lobbyServer = std::nullopt);

/// The activity of Discord of what the player is doing, in the language of the game:
///
/// | Place | Details | State | Party | Time |
/// |---|---|---|---|---|
/// | Hangar | "In the hangar" | | | |
/// | Ships | "In the hangar" | "Choosing a ship" | | |
/// | Settings | "In the settings" | | | |
/// | Lore | "In the hangar" | "Reading the lore" | | |
/// | Quests | "In the hangar" | "Checking the quests" | | |
/// | Servers | "Online" | "Choosing a server" | | |
/// | SigningIn | "Online" | "Signing in" | | |
/// | Account | "Online" | "Managing the account" | | |
/// | Online | "Online" | "Choosing a game" | 1/4 | |
/// | Solo | "Solo" | "Wave 7/12 · Boss fight" | | since the start |
/// | OnlineGame | "Online game" | "Wave 7 · Boss fight" | 3/4 | since the start |
/// | OnlineResults | "Online game" | "Game over · 31 waves cleared" | | |
///
/// The list of games of a server is joinable too: a party of one, its join secret leads to the server
/// (GameInvite without a game). A game online is joinable while it has room: its party has the id of the game
/// (partyId()), and the activity its join secret (inviteSecret()), which Discord gives only to the friends
/// invited or accepted.
///
/// In a game, the wave, then what happens in it: the victory or the defeat once it ended, else the final
/// battle, else the pause, else the boss fight. No wave started, the wave is left out. The large image is the
/// logo of the game (PresenceLogoImage), "R-Type" over it; the small one, in its corner, in a game only, the
/// ship equipped, its code and name over it. Nothing that tells where the player is: neither the address of
/// the server, nor the name of the game.
[[nodiscard]] discord::Activity describeActivity(const PlayerActivity &activity,
                                                 const localization::Localization &translations);

/// Keeps the activity of the player in Discord up to date: called every frame, it updates the presence when
/// the activity changes, at most once every MinInterval (Discord limits a presence to 5 updates in 20 s), the
/// last change sent once the interval passed. An activity Discord refused (not running, or connecting with a
/// token) is sent again every RetryInterval until it takes it. Off, it clears the presence and sends nothing.
///
/// @code
/// tracker.update(observeActivity(engine, sessions, ships), std::chrono::steady_clock::now());
/// @endcode
class PresenceTracker
{
public:
    using Clock = std::chrono::steady_clock;
    static constexpr std::chrono::seconds MinInterval{4};
    static constexpr std::chrono::seconds RetryInterval{15};

    /// @param presence Outlives the tracker.
    /// @param translations The language of the texts, followed when it changes. Outlives the tracker.
    PresenceTracker(discord::Presence &presence, const localization::Localization &translations,
                    bool enabled = true);

    /// What the player is doing now.
    void update(const PlayerActivity &activity, Clock::time_point now);
    /// Shows the activity again from the next update, or clears it.
    void setEnabled(bool enabled);
    /// Sends the activity again at the next update, at once: Discord lost the one shown, once connected with
    /// a token (Connect() clears it) or disconnected from it, and shows nothing until then.
    void resend();
    [[nodiscard]] bool isEnabled() const { return _enabled; }

private:
    /// What Discord answered for the last update sent, shared with the callback of the presence, which may
    /// run after the tracker is gone.
    struct Answer {
        /// Counts the updates sent: the answer to an older one is dropped.
        std::uint64_t attempt = 0;
        bool refused = false;
    };

    discord::Presence &_presence;
    const localization::Localization &_translations;
    bool _enabled;
    /// The activity Discord was given last, none once cleared or refused, and when.
    std::optional<discord::Activity> _sent;
    std::optional<Clock::time_point> _sentAt;
    /// Not before this time: Discord refused the last activity.
    std::optional<Clock::time_point> _retryAt;
    std::shared_ptr<Answer> _answer = std::make_shared<Answer>();
};

/// The choice of the Settings page kept in the settings: on when it is missing.
[[nodiscard]] bool readDiscordActivity(const engine::Settings &settings);

} // namespace rtype::client
