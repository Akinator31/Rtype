#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

/// The Discord friends of the player in the game (docs/architecture/client-online.md#friends): who they are,
/// whether they play R-Type, inviting them to the game played, asking to join theirs, and the invitations
/// received. It needs the client connected to Discord with a token (Connection, Ready): before, there is no
/// friend.
namespace rtype::discord
{

/// Where a friend is, the friends playing R-Type first.
enum class FriendPresence : std::uint8_t {
    PlayingRType,
    Online,
    Offline,
};

/// A Discord friend of the player.
struct Friend {
    std::uint64_t id = 0;
    /// Its display name, else its username.
    std::string name = {};
    FriendPresence presence = FriendPresence::Offline;
    /// What it does in R-Type, as its activity tells it ("Online game · Wave 7"): empty when it does not
    /// play.
    std::string activity = {};
    /// Its game has room: asking to join it can work.
    bool joinable = false;
    /// Its picture on the CDN of Discord, a small PNG: one of the default pictures of Discord if it has none.
    std::string avatarUrl = {};

    friend bool operator==(const Friend &, const Friend &) = default;
};

/// An invitation a friend sent the player, through Discord.
struct Invitation {
    enum class Kind : std::uint8_t {
        /// The friend invites the player into its game: accepting joins it.
        Invite,
        /// The friend asks to join the game of the player: accepting lets it in.
        JoinRequest,
    };

    /// Its message in Discord: the same invitation keeps it.
    std::uint64_t id = 0;
    std::uint64_t from = 0;
    /// The name of the friend.
    std::string name = {};
    Kind kind = Kind::Invite;

    friend bool operator==(const Invitation &, const Invitation &) = default;
};

/// The friends of the player, and the invitations between them.
///
/// @code
/// friends.start(); // once connected to Discord with a token
/// friends.onChanged([&] { showFriends(friends.list(), friends.invitations()); });
/// friends.askToJoin(friendId, [](bool sent) { /* the friend accepts in its game, or in Discord */ });
/// @endcode
///
/// Everything runs on the thread that polls Discord: in the client, the game loop. Every answer comes at a
/// poll, never inside the call.
class Friends
{
public:
    /// Whether Discord took the request.
    using Sent = std::function<void(bool sent)>;
    /// An invitation accepted: the join secret of the game of the friend for an Invite, none for a
    /// JoinRequest, which only lets the friend in. Nothing when Discord refused it.
    using Accepted = std::function<void(bool accepted, std::optional<std::string> secret)>;

    virtual ~Friends() = default;

    /// Starts following the friends: the client is connected to Discord with a token.
    virtual void start() = 0;
    /// Forgets them: the client is no longer connected.
    virtual void stop() = 0;
    /// The friends are known: started, and connected.
    [[nodiscard]] virtual bool ready() const = 0;

    /// The friends, those playing R-Type first, then those online, then the others.
    [[nodiscard]] virtual std::vector<Friend> list() const = 0;
    /// The invitations received and still valid, oldest first.
    [[nodiscard]] virtual std::vector<Invitation> invitations() const = 0;
    /// Called at a poll when the friends or the invitations change.
    virtual void onChanged(std::function<void()> changed) = 0;

    /// Invites a friend into the game the player is in: its activity must be joinable.
    virtual void invite(std::uint64_t friendId, Sent sent) = 0;
    /// Asks a friend to join its game, which must have room.
    virtual void askToJoin(std::uint64_t friendId, Sent sent) = 0;
    /// Accepts an invitation of invitations().
    virtual void accept(std::uint64_t invitationId, Accepted accepted) = 0;
    /// Forgets an invitation of invitations(): the friend is not told.
    virtual void dismiss(std::uint64_t invitationId) = 0;
};

} // namespace rtype::discord
