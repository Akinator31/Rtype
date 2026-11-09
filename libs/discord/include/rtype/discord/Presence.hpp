#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

/// The activity of the player in its Discord profile (docs/architecture/client-online.md#discord): R-Type and
/// what the player does in it. It needs no token: the Discord client of the player shows it. The client and
/// its tests know only this interface; its implementation on the Social SDK comes with the rich presence.
namespace rtype::discord
{

/// The players of the game the player is in.
struct Party {
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
    /// The key of an image uploaded on the Rich Presence page of the application: the ship.
    std::string largeImage;
    /// Shown over the image: the name of the ship.
    std::string largeText;

    friend bool operator==(const Activity &, const Activity &) = default;
};

/// Shows the activity of the player in Discord.
///
/// @code
/// rtype::discord::Activity activity;
/// activity.details = "Dans le hangar";
/// presence.update(activity);
/// // Leaving the game, or the setting turned off
/// presence.clear();
/// @endcode
///
/// Everything runs on the thread that polls Discord: in the client, the game loop. Without Discord running,
/// it does nothing.
class Presence
{
public:
    virtual ~Presence() = default;

    /// Shows `activity`, in place of the one shown.
    virtual void update(const Activity &activity) = 0;
    /// Shows nothing.
    virtual void clear() = 0;
};

} // namespace rtype::discord
