#include "Discord/DiscordFriends.hpp"

#include <algorithm>

namespace rtype::client
{

namespace
{

std::string presenceText(const discord::Friend &entry, const localization::Localization &translations)
{
    switch (entry.presence) {
    case discord::FriendPresence::PlayingRType:
        return entry.activity.empty() ? translations.text("friends.playing") : entry.activity;
    case discord::FriendPresence::Online:
        return translations.text("friends.online");
    case discord::FriendPresence::Offline:
        break;
    }
    return translations.text("friends.offline");
}

} // namespace

std::vector<FriendRow> friendRows(const discord::Friends &friends, FriendsView view,
                                  const localization::Localization &translations, std::size_t most)
{
    std::vector<FriendRow> rows;
    const auto full = [&rows, most] { return rows.size() >= most; };
    // What waits for an answer first: an invitation into a game in the lobby, a request to join in a game
    const discord::Invitation::Kind kind = view == FriendsView::Lobby
                                               ? discord::Invitation::Kind::Invite
                                               : discord::Invitation::Kind::JoinRequest;
    const std::vector<discord::Friend> list = friends.list();
    const auto pictureOf = [&list](std::uint64_t id) {
        const auto found = std::ranges::find(list, id, &discord::Friend::id);
        return found == list.end() ? std::string() : found->avatarUrl;
    };
    for (const discord::Invitation &invitation : friends.invitations()) {
        if (full()) {
            return rows;
        }
        if (invitation.kind != kind) {
            continue;
        }
        const bool invite = kind == discord::Invitation::Kind::Invite;
        rows.push_back({.action = invite ? FriendRow::Action::AcceptInvite : FriendRow::Action::AcceptRequest,
                        .id = invitation.id,
                        .name = invitation.name,
                        .detail = translations.text(invite ? "friends.invites_you" : "friends.asks_to_join"),
                        .side = translations.text(invite ? "friends.join" : "friends.accept"),
                        .online = true,
                        .avatarUrl = pictureOf(invitation.from)});
    }
    for (const discord::Friend &entry : list) {
        if (full()) {
            break;
        }
        const bool online = entry.presence != discord::FriendPresence::Offline;
        FriendRow row{.id = entry.id,
                      .name = entry.name,
                      .detail = presenceText(entry, translations),
                      .online = online,
                      .avatarUrl = entry.avatarUrl};
        if (view == FriendsView::Lobby) {
            if (entry.presence == discord::FriendPresence::PlayingRType && entry.joinable) {
                row.action = FriendRow::Action::AskToJoin;
                row.side = translations.text("friends.ask_to_join");
            } else if (online) {
                // Invited to the server the player is on
                row.action = FriendRow::Action::Invite;
                row.side = translations.text("friends.invite");
            }
        } else {
            // Offline, a friend cannot be invited
            if (!online) {
                continue;
            }
            row.action = FriendRow::Action::Invite;
            row.side = translations.text("friends.invite");
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

} // namespace rtype::client
