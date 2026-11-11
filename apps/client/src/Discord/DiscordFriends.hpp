#pragma once

#include "rtype/discord/Friends.hpp"
#include "rtype/localization/Localization.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/// The Discord friends of the player in the game (docs/architecture/client-online.md#friends): the rows of
/// the lobby of a server, where the player joins its friends, and of the pause menu of a game online, where
/// it invites them.
namespace rtype::client
{

/// One row of friends, in the language of the game.
struct FriendRow {
    enum class Action : std::uint8_t {
        /// Nothing to do: a friend offline, or one whose game is full.
        None,
        /// An invitation of a friend into its game: accepting joins it.
        AcceptInvite,
        /// A friend asks to join the game of the player: accepting lets it in.
        AcceptRequest,
        /// A friend plays R-Type with room in its game: asking to join it.
        AskToJoin,
        /// A friend online, invited into the game of the player.
        Invite,
    };

    Action action = Action::None;
    /// The invitation for AcceptInvite and AcceptRequest, else the friend.
    std::uint64_t id = 0;
    std::string name = {};
    /// Under the name: what it does, or where it is.
    std::string detail = {};
    /// On the right: the action, empty for None.
    std::string side = {};
    /// Its presence: a friend offline is dimmed.
    bool online = false;
    /// Its picture on the CDN of Discord, empty if unknown.
    std::string avatarUrl = {};

    friend bool operator==(const FriendRow &, const FriendRow &) = default;
};

/// Where the friends are shown.
enum class FriendsView : std::uint8_t {
    /// The lobby of a server: the invitations into the games of friends, then the friends, those playing
    /// first, joined when their game has room, the others online invited to the server.
    Lobby,
    /// The pause menu of a game online: the friends asking to join, then the friends online, invited.
    Game,
};

/// The rows of the friends, at most `most`: the invitations first, oldest first.
[[nodiscard]] std::vector<FriendRow> friendRows(const discord::Friends &friends, FriendsView view,
                                                const localization::Localization &translations,
                                                std::size_t most);

} // namespace rtype::client
