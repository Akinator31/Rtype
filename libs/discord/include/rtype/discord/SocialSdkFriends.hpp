#pragma once

#include "rtype/discord/Friends.hpp"

#include <memory>

namespace rtype::discord
{

class Session;

/// The Friends of the player through the Discord Social SDK: the relationships of the client of the Session
/// (`GetRelationshipsByGroup`), its activity invites (`SendActivityInvite`, `SendActivityJoinRequest`,
/// `AcceptActivityInvite`, `SendActivityJoinRequestReply`), the invitations received
/// (`SetActivityInviteCreatedCallback`). The client must be connected with a token: start() once it is Ready.
class SocialSdkFriends final : public Friends
{
public:
    /// What the callbacks of the SDK reach: defined in the implementation.
    struct State;

private:
    Session &_session;
    std::shared_ptr<State> _state;

public:
    /// Follows the friends through the client of `session`, which outlives it.
    explicit SocialSdkFriends(Session &session);
    ~SocialSdkFriends() override;

    SocialSdkFriends(const SocialSdkFriends &) = delete;
    SocialSdkFriends &operator=(const SocialSdkFriends &) = delete;

    void start() override;
    void stop() override;
    [[nodiscard]] bool ready() const override;
    [[nodiscard]] std::vector<Friend> list() const override;
    [[nodiscard]] std::vector<Invitation> invitations() const override;
    void onChanged(std::function<void()> changed) override;
    void invite(std::uint64_t friendId, Sent sent) override;
    void askToJoin(std::uint64_t friendId, Sent sent) override;
    void accept(std::uint64_t invitationId, Accepted accepted) override;
    void dismiss(std::uint64_t invitationId) override;
};

} // namespace rtype::discord
