#include "rtype/discord/SocialSdkFriends.hpp"

#include "rtype/discord/Session.hpp"
#include "rtype/log/Logger.hpp"

#include <discordpp.h>

#include <algorithm>
#include <map>
#include <utility>

namespace rtype::discord
{

namespace
{

const log::Logger &logger()
{
    static const log::Logger instance("discord");
    return instance;
}

/// "Online game · Wave 7": what the activity of a friend in R-Type tells.
std::string activityText(const discordpp::Activity &activity)
{
    std::string text = activity.Details().value_or(std::string());
    if (const std::optional<std::string> state = activity.State(); state && !state->empty()) {
        text += text.empty() ? *state : " · " + *state;
    }
    return text;
}

/// The picture of a user as a PNG of 64 pixels: what a row of friends shows.
std::string pictureUrl(const discordpp::UserHandle &user)
{
    std::string url =
        user.AvatarUrl(discordpp::UserHandle::AvatarType::Png, discordpp::UserHandle::AvatarType::Png);
    if (!url.empty()) {
        url += url.find('?') == std::string::npos ? "?size=64" : "&size=64";
    }
    return url;
}

} // namespace

struct SocialSdkFriends::State {
    bool started = false;
    std::function<void()> changed;
    /// The invitations received and still valid, by the id of their message, and the order they came in.
    std::map<std::uint64_t, discordpp::ActivityInvite> invites;
    std::vector<std::uint64_t> order;

    void tell() const
    {
        if (started && changed) {
            changed();
        }
    }

    void forget(std::uint64_t id)
    {
        invites.erase(id);
        std::erase(order, id);
    }
};

SocialSdkFriends::SocialSdkFriends(Session &session) : _session(session), _state(std::make_shared<State>()) {}

SocialSdkFriends::~SocialSdkFriends()
{
    _state->changed = nullptr;
}

void SocialSdkFriends::start()
{
    if (_state->started) {
        return;
    }
    _state->started = true;
    discordpp::Client &client = _session.client();
    const auto changed = [state = std::weak_ptr<State>(_state)] {
        if (const std::shared_ptr<State> alive = state.lock()) {
            alive->tell();
        }
    };
    client.SetRelationshipCreatedCallback([changed](std::uint64_t, bool) { changed(); });
    client.SetRelationshipDeletedCallback([changed](std::uint64_t, bool) { changed(); });
    client.SetRelationshipGroupsUpdatedCallback([changed](std::uint64_t) { changed(); });
    client.SetUserUpdatedCallback([changed](std::uint64_t) { changed(); });
    client.SetActivityInviteCreatedCallback(
        [state = std::weak_ptr<State>(_state)](discordpp::ActivityInvite invite) {
            const std::shared_ptr<State> alive = state.lock();
            if (!alive || !alive->started || !invite.IsValid()) {
                return;
            }
            const std::uint64_t id = invite.MessageId();
            if (!alive->invites.contains(id)) {
                alive->order.push_back(id);
            }
            alive->invites.insert_or_assign(id, std::move(invite));
            alive->tell();
        });
    client.SetActivityInviteUpdatedCallback(
        [state = std::weak_ptr<State>(_state)](discordpp::ActivityInvite invite) {
            const std::shared_ptr<State> alive = state.lock();
            if (!alive || !alive->started) {
                return;
            }
            const std::uint64_t id = invite.MessageId();
            if (!invite.IsValid()) {
                // The friend left its game, or went offline
                alive->forget(id);
            } else if (alive->invites.contains(id)) {
                alive->invites.insert_or_assign(id, std::move(invite));
            }
            alive->tell();
        });
    logger().info("Following the Discord friends of the player");
    _state->tell();
}

void SocialSdkFriends::stop()
{
    if (!_state->started) {
        return;
    }
    _state->started = false;
    _state->invites.clear();
    _state->order.clear();
    // Told once there is nothing left
    if (_state->changed) {
        _state->changed();
    }
}

bool SocialSdkFriends::ready() const
{
    return _state->started;
}

std::vector<Friend> SocialSdkFriends::list() const
{
    std::vector<Friend> friends;
    if (!_state->started) {
        return friends;
    }
    const discordpp::Client &client = _session.client();
    const std::pair<discordpp::RelationshipGroupType, FriendPresence> groups[] = {
        {discordpp::RelationshipGroupType::OnlinePlayingGame, FriendPresence::PlayingRType},
        {discordpp::RelationshipGroupType::OnlineElsewhere, FriendPresence::Online},
        {discordpp::RelationshipGroupType::Offline, FriendPresence::Offline},
    };
    for (const auto &[group, presence] : groups) {
        for (const discordpp::RelationshipHandle &relationship : client.GetRelationshipsByGroup(group)) {
            if (relationship.DiscordRelationshipType() != discordpp::RelationshipType::Friend) {
                continue;
            }
            const std::optional<discordpp::UserHandle> user = relationship.User();
            if (!user) {
                continue;
            }
            Friend entry{.id = relationship.Id(), .name = user->DisplayName(), .presence = presence};
            entry.avatarUrl = pictureUrl(*user);
            if (const std::optional<discordpp::Activity> activity = user->GameActivity()) {
                entry.activity = activityText(*activity);
                if (const std::optional<discordpp::ActivityParty> party = activity->Party()) {
                    entry.joinable = party->CurrentSize() < party->MaxSize();
                }
            }
            friends.push_back(std::move(entry));
        }
    }
    return friends;
}

std::vector<Invitation> SocialSdkFriends::invitations() const
{
    std::vector<Invitation> result;
    if (!_state->started) {
        return result;
    }
    discordpp::Client &client = _session.client();
    for (const std::uint64_t id : _state->order) {
        const discordpp::ActivityInvite &invite = _state->invites.at(id);
        Invitation entry{.id = id,
                         .from = invite.SenderId(),
                         .kind = invite.Type() == discordpp::ActivityActionTypes::JoinRequest
                                     ? Invitation::Kind::JoinRequest
                                     : Invitation::Kind::Invite};
        const std::optional<discordpp::UserHandle> sender = client.GetUser(invite.SenderId());
        entry.name = sender ? sender->DisplayName() : std::string("?");
        result.push_back(std::move(entry));
    }
    return result;
}

void SocialSdkFriends::onChanged(std::function<void()> changed)
{
    _state->changed = std::move(changed);
}

void SocialSdkFriends::invite(std::uint64_t friendId, Sent sent)
{
    if (!_state->started) {
        return;
    }
    _session.client().SendActivityInvite(
        friendId, "", [sent = std::move(sent)](const discordpp::ClientResult &result) {
            if (!result.Successful()) {
                logger().info("Discord sent no invitation: {}", result.Error());
            }
            if (sent) {
                sent(result.Successful());
            }
        });
}

void SocialSdkFriends::askToJoin(std::uint64_t friendId, Sent sent)
{
    if (!_state->started) {
        return;
    }
    _session.client().SendActivityJoinRequest(
        friendId, [sent = std::move(sent)](const discordpp::ClientResult &result) {
            if (!result.Successful()) {
                logger().info("Discord sent no request to join: {}", result.Error());
            }
            if (sent) {
                sent(result.Successful());
            }
        });
}

void SocialSdkFriends::accept(std::uint64_t invitationId, Accepted accepted)
{
    const auto found = _state->invites.find(invitationId);
    if (!_state->started || found == _state->invites.end()) {
        return;
    }
    discordpp::ActivityInvite invite = found->second;
    _state->forget(invitationId);
    _state->tell();
    discordpp::Client &client = _session.client();
    if (invite.Type() == discordpp::ActivityActionTypes::JoinRequest) {
        // The friend gets an invitation in return, which it accepts in turn
        client.SendActivityJoinRequestReply(
            std::move(invite), [accepted = std::move(accepted)](const discordpp::ClientResult &result) {
                if (accepted) {
                    accepted(result.Successful(), std::nullopt);
                }
            });
        return;
    }
    client.AcceptActivityInvite(
        std::move(invite),
        [accepted = std::move(accepted)](const discordpp::ClientResult &result, std::string secret) {
            if (!result.Successful()) {
                logger().info("Discord did not accept the invitation: {}", result.Error());
            }
            if (accepted) {
                accepted(result.Successful(),
                         result.Successful() ? std::optional<std::string>(std::move(secret)) : std::nullopt);
            }
        });
}

void SocialSdkFriends::dismiss(std::uint64_t invitationId)
{
    if (_state->invites.contains(invitationId)) {
        _state->forget(invitationId);
        _state->tell();
    }
}

} // namespace rtype::discord
