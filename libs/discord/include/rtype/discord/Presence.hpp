#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

/// The activity of the player in its Discord profile (docs/architecture/client-online.md#discord): R-Type and
/// what the player does in it. It needs no token: the Discord client of the player shows it. The client and
/// its tests know only this interface; its implementation on the Social SDK comes with the rich presence.
namespace rtype::discord
{

/// The players of the game the player is in.
struct Party {
    /// The same for all the players of the game: Discord groups them. Empty, a party of its own.
    std::string id = {};
    std::uint8_t size = 0;
    std::uint8_t max = 0;

    friend bool operator==(const Party &, const Party &) = default;
};

/// What the profile of the player shows. Empty texts are not shown.
struct Activity {
    /// The second line: "Partie · vague 7/12".
    std::string state;
    /// The first line: "En ligne · <server name>".
    std::string details;
    /// The game of the player, for the "3 of 4" of Discord.
    std::optional<Party> party;
    /// When the game started, for the time Discord counts since.
    std::optional<std::chrono::system_clock::time_point> startedAt;
    /// The key of an image uploaded on the Rich Presence page of the application: the logo of the game.
    std::string largeImage;
    /// Shown over the large image: the name of the game.
    std::string largeText;
    /// The key of the small image, in the corner of the large one: the ship.
    std::string smallImage;
    /// Shown over the small image: the code and the name of the ship.
    std::string smallText;
    /// Joinable: Discord gives this secret to the friends the player invites, or accepts. It needs a party
    /// with room in it. Empty, not joinable.
    std::string joinSecret;

    friend bool operator==(const Activity &, const Activity &) = default;
};

/// Shows the activity of the player in Discord.
///
/// @code
/// rtype::discord::Activity activity;
/// activity.details = "Dans le hangar";
/// presence.update(activity, [](bool shown) { /* refused: send it again later */ });
/// // Leaving the game, or the setting turned off
/// presence.clear();
/// @endcode
///
/// Everything runs on the thread that polls Discord: in the client, the game loop. Without Discord running,
/// it shows nothing, and says so.
class Presence
{
public:
    /// Whether Discord took the activity: false when it refused it, or could not be reached.
    using Shown = std::function<void(bool shown)>;

    virtual ~Presence() = default;

    /// Shows `activity`, in place of the one shown. `shown`, if any, is told what Discord answered, at a
    /// later poll, never inside the call.
    virtual void update(const Activity &activity, Shown shown) = 0;
    /// Shows nothing.
    virtual void clear() = 0;

    /// The player accepted, in Discord, to join the activity of a friend: its join secret.
    using JoinAccepted = std::function<void(const std::string &secret)>;
    /// Calls `joined` with the secret of each activity the player accepts to join from now on, at a poll.
    /// From the start of the game: Discord launches it to join, and gives the secret once it runs. Discord is
    /// told how to launch the game.
    virtual void onJoin(JoinAccepted joined) = 0;
};

} // namespace rtype::discord
