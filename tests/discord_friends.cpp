// Tests of the Discord friends in the game (apps/client/src/Discord/DiscordFriends.hpp): the rows of the
// lobby, where the player joins its friends, and of the pause menu, where it invites them, against the fake
// Social SDK (docs/architecture/client-online.md#friends).

#include "Discord/DiscordFriends.hpp"
#include "Discord/FriendPictures.hpp"
#include "FakeSocialSdk.hpp"
#include "rtype/localization/Localization.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

using rtype::client::FriendRow;
using rtype::client::FriendsView;
using rtype::discord::Friend;
using rtype::discord::FriendPresence;
using rtype::discord::Invitation;

namespace
{

rtype::localization::Localization english()
{
    rtype::localization::Localization translations("en");
    translations.load("en", std::filesystem::path(RTYPE_TEST_LOCALES) / "en.lang");
    return translations;
}

void befriend(testing::FakeSocialSdk &sdk)
{
    sdk.friends.friends = {
        {.id = 1,
         .name = "Ada",
         .presence = FriendPresence::PlayingRType,
         .activity = "Online game · Wave 7",
         .joinable = true},
        {.id = 2, .name = "Bob", .presence = FriendPresence::PlayingRType, .activity = "Solo"},
        {.id = 3,
         .name = "Cyd",
         .presence = FriendPresence::Online,
         .avatarUrl = "https://cdn.discordapp.com/avatars/3/abc.png?size=64"},
        {.id = 4, .name = "Dee", .presence = FriendPresence::Offline},
    };
    sdk.friends.received = {
        {.id = 100, .from = 3, .name = "Cyd", .kind = Invitation::Kind::Invite},
        {.id = 101, .from = 4, .name = "Dee", .kind = Invitation::Kind::JoinRequest},
    };
    sdk.friends.start();
}

} // namespace

TEST_CASE("Discord friends: none before the connection with a token", "[discord]")
{
    const rtype::localization::Localization translations = english();
    testing::FakeSocialSdk sdk;
    sdk.friends.friends = {{.id = 1, .name = "Ada", .presence = FriendPresence::Online}};
    CHECK_FALSE(sdk.friends.ready());
    CHECK(friendRows(sdk.friends, FriendsView::Lobby, translations, 10).empty());
}

TEST_CASE("Discord friends: the lobby shows the invitations, then the friends who can be joined", "[discord]")
{
    const rtype::localization::Localization translations = english();
    testing::FakeSocialSdk sdk;
    befriend(sdk);

    const std::vector<FriendRow> rows = friendRows(sdk.friends, FriendsView::Lobby, translations, 10);
    REQUIRE(rows.size() == 5);
    CHECK(rows[0] == FriendRow{.action = FriendRow::Action::AcceptInvite,
                               .id = 100,
                               .name = "Cyd",
                               .detail = "invites you to their game",
                               .side = "Join",
                               .online = true,
                               .avatarUrl = "https://cdn.discordapp.com/avatars/3/abc.png?size=64"});
    CHECK(rows[1] == FriendRow{.action = FriendRow::Action::AskToJoin,
                               .id = 1,
                               .name = "Ada",
                               .detail = "Online game · Wave 7",
                               .side = "Ask to join",
                               .online = true});
    // Playing alone, or a full game: nothing to ask
    // Playing alone, or online elsewhere: invited to the server
    CHECK(rows[2].action == FriendRow::Action::Invite);
    CHECK(rows[2].side == "Invite");
    CHECK(rows[2].detail == "Solo");
    CHECK(rows[3].action == FriendRow::Action::Invite);
    CHECK(rows[3].detail == "Online");
    CHECK(rows[4] == FriendRow{.id = 4, .name = "Dee", .detail = "Offline", .online = false});

    CHECK(friendRows(sdk.friends, FriendsView::Lobby, translations, 2).size() == 2);
}

TEST_CASE("Discord friends: the pause menu shows the requests to join, then the friends to invite",
          "[discord]")
{
    const rtype::localization::Localization translations = english();
    testing::FakeSocialSdk sdk;
    befriend(sdk);

    const std::vector<FriendRow> rows = friendRows(sdk.friends, FriendsView::Game, translations, 10);
    REQUIRE(rows.size() == 4);
    CHECK(rows[0].action == FriendRow::Action::AcceptRequest);
    CHECK(rows[0].id == 101);
    CHECK(rows[0].side == "Accept");
    // Offline, Dee cannot be invited
    for (std::size_t index = 1; index < rows.size(); ++index) {
        CHECK(rows[index].action == FriendRow::Action::Invite);
        CHECK(rows[index].online);
    }
}

TEST_CASE("Discord friends: what the game sends is answered at the next poll", "[discord]")
{
    testing::FakeSocialSdk sdk;
    befriend(sdk);
    int changes = 0;
    sdk.friends.onChanged([&changes] { ++changes; });
    sdk.poll();
    CHECK(changes == 1);

    std::optional<bool> invited;
    sdk.friends.invite(3, [&invited](bool sent) { invited = sent; });
    CHECK_FALSE(invited);
    sdk.poll();
    CHECK(invited == true);
    CHECK(sdk.friends.invited == std::vector<std::uint64_t>{3});

    sdk.friends.secrets[100] = "rtype1:7:rtype.example.com:4242";
    std::optional<std::string> secret;
    sdk.friends.accept(100, [&secret](bool accepted, const std::optional<std::string> &joined) {
        CHECK(accepted);
        secret = joined;
    });
    sdk.poll();
    CHECK(secret == "rtype1:7:rtype.example.com:4242");
    CHECK(sdk.friends.invitations().size() == 1);
    CHECK(changes == 2);

    sdk.friends.refusing = true;
    std::optional<bool> asked;
    sdk.friends.askToJoin(1, [&asked](bool sent) { asked = sent; });
    sdk.poll();
    CHECK(asked == false);

    sdk.friends.stop();
    sdk.poll();
    CHECK(sdk.friends.list().empty());
}

TEST_CASE("Discord friends: every text is in English and in French", "[discord]")
{
    for (const std::string language : {"en", "fr"}) {
        CAPTURE(language);
        rtype::localization::Localization translations(language);
        translations.load(language, std::filesystem::path(RTYPE_TEST_LOCALES) / (language + ".lang"));
        for (const char *key :
             {"friends.title", "friends.none", "friends.not_connected", "friends.playing", "friends.online",
              "friends.offline", "friends.invites_you", "friends.asks_to_join", "friends.join",
              "friends.accept", "friends.ask_to_join", "friends.invite", "friends.invite_sent",
              "friends.request_sent", "friends.not_sent", "friends.new_invite", "friends.new_request",
              "game.group.friends"}) {
            CAPTURE(key);
            CHECK(translations.text(key) != key);
        }
    }
}

TEST_CASE("Discord friends: each row has the picture of its friend", "[discord]")
{
    const rtype::localization::Localization translations = english();
    testing::FakeSocialSdk sdk;
    befriend(sdk);
    const std::vector<FriendRow> rows = friendRows(sdk.friends, FriendsView::Lobby, translations, 10);
    // The invitation of Cyd, then Cyd itself
    CHECK(rows[0].avatarUrl == "https://cdn.discordapp.com/avatars/3/abc.png?size=64");
    CHECK(rows[3].avatarUrl == "https://cdn.discordapp.com/avatars/3/abc.png?size=64");
    CHECK(rows[1].avatarUrl.empty());
}

TEST_CASE("Discord friends: pictures come from the CDN of Discord only", "[discord]")
{
    CHECK(rtype::client::isDiscordPicture("https://cdn.discordapp.com/avatars/3/abc.png?size=64"));
    CHECK(rtype::client::isDiscordPicture("https://cdn.discordapp.com/embed/avatars/1.png"));
    for (const char *url : {"", "https://cdn.discordapp.com/", "http://cdn.discordapp.com/avatars/3/abc.png",
                            "https://cdn.discordapp.com.evil.example/a.png",
                            "https://evil.example/cdn.discordapp.com/", "file:///etc/passwd"}) {
        CAPTURE(url);
        CHECK_FALSE(rtype::client::isDiscordPicture(url));
    }
}
