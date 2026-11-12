// Tests of the invitations to a game online from Discord (apps/client/src/Discord/DiscordInvite.hpp): the
// join secret of a game and its party, and what the presence of a game online carries for them
// (docs/architecture/client-online.md#invitations).

#include "Discord/DiscordInvite.hpp"
#include "Discord/DiscordPresence.hpp"
#include "FakeSocialSdk.hpp"
#include "rtype/localization/Localization.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>

using rtype::client::GameInvite;
using rtype::client::PlayerActivity;

TEST_CASE("Discord invite: the secret of a game gives the game back", "[discord]")
{
    for (const GameInvite &invite : {GameInvite{.server = "rtype.example.com:4242", .gameId = 7},
                                     GameInvite{.server = "127.0.0.1:4242", .gameId = 0},
                                     GameInvite{.server = "[::1]:4242", .gameId = 4'000'000'000U}}) {
        CAPTURE(invite.server, invite.gameId);
        const auto secret = rtype::client::inviteSecret(invite);
        REQUIRE(secret);
        CHECK(secret->size() <= rtype::client::MaxDiscordSecretLength);
        CHECK(rtype::client::parseInviteSecret(*secret) == invite);
    }
    CHECK(rtype::client::inviteSecret({.server = "rtype.example.com:4242", .gameId = 7}) ==
          "rtype1:7:rtype.example.com:4242");
    // The lobby of the server, without a game
    const GameInvite lobby{.server = "rtype.example.com:4242"};
    CHECK(rtype::client::inviteSecret(lobby) == "rtype1::rtype.example.com:4242");
    CHECK(rtype::client::parseInviteSecret("rtype1::rtype.example.com:4242") == lobby);
    CHECK(rtype::client::partyId(lobby) !=
          rtype::client::partyId({.server = "rtype.example.com:4242", .gameId = 0}));
}

TEST_CASE("Discord invite: what is not the secret of a game is refused", "[discord]")
{
    const std::vector<std::string> secrets{"",
                                           "rtype1:",
                                           "rtype1:7",
                                           "rtype1:7:",
                                           "rtype1:x:host:4242",
                                           "rtype2:7:host:4242",
                                           "rtype1:7:host:99999",
                                           "rtype1:7:host name:4242",
                                           "rtype1:-1:host:4242",
                                           "lobby-secret-of-another-game",
                                           "rtype1:7:" + std::string(200, 'a')};
    for (const std::string &secret : secrets) {
        CAPTURE(secret);
        CHECK_FALSE(rtype::client::parseInviteSecret(secret));
    }
    // A server too long for Discord has no secret
    CHECK_FALSE(rtype::client::inviteSecret({.server = std::string(130, 'a') + ":4242", .gameId = 1}));
}

TEST_CASE("Discord invite: the party of a game is the same for all its players, and tells nothing",
          "[discord]")
{
    const GameInvite game{.server = "rtype.example.com:4242", .gameId = 7};
    const std::string party = rtype::client::partyId(game);
    CHECK(party == rtype::client::partyId(game));
    CHECK(party != rtype::client::partyId({.server = "rtype.example.com:4242", .gameId = 8}));
    CHECK(party != rtype::client::partyId({.server = "other.example.com:4242", .gameId = 7}));
    CHECK(party.starts_with("rtype-"));
    CHECK(party.size() <= rtype::client::MaxDiscordSecretLength);
    CHECK(party.find("example") == std::string::npos);
}

TEST_CASE("Discord invite: the list of games of a server is joinable, a party of one", "[discord]")
{
    rtype::localization::Localization translations("en");
    translations.load("en", std::filesystem::path(RTYPE_TEST_LOCALES) / "en.lang");
    PlayerActivity activity;
    activity.place = PlayerActivity::Place::Online;
    // Out of the lobby of a server: nothing to join
    CHECK(describeActivity(activity, translations).joinSecret.empty());
    CHECK_FALSE(describeActivity(activity, translations).party);

    activity.game = GameInvite{.server = "rtype.example.com:4242"};
    activity.players = 1;
    activity.maxPlayers = 4;
    const rtype::discord::Activity lobby = describeActivity(activity, translations);
    CHECK(lobby.joinSecret == "rtype1::rtype.example.com:4242");
    REQUIRE(lobby.party);
    CHECK(lobby.party->size == 1);
    CHECK(lobby.party->max == 4);
    CHECK(lobby.state == "Choosing a game");
}

TEST_CASE("Discord invite: a game online with room is joinable", "[discord]")
{
    rtype::localization::Localization translations("en");
    translations.load("en", std::filesystem::path(RTYPE_TEST_LOCALES) / "en.lang");
    PlayerActivity activity;
    activity.place = PlayerActivity::Place::OnlineGame;
    activity.players = 2;
    activity.maxPlayers = 4;
    activity.game = GameInvite{.server = "rtype.example.com:4242", .gameId = 7};

    const rtype::discord::Activity joinable = describeActivity(activity, translations);
    REQUIRE(joinable.party);
    CHECK(joinable.party->id == rtype::client::partyId(*activity.game));
    CHECK(joinable.joinSecret == "rtype1:7:rtype.example.com:4242");
    // Never in what the profile shows
    CHECK(joinable.state.find("example") == std::string::npos);
    CHECK(joinable.details.find("example") == std::string::npos);

    activity.players = 4;
    CHECK(describeActivity(activity, translations).joinSecret.empty());
    activity.players = 2;
    activity.outcome = rtype::game::GameOutcome::Defeat;
    CHECK(describeActivity(activity, translations).joinSecret.empty());

    PlayerActivity solo;
    solo.place = PlayerActivity::Place::Solo;
    CHECK(describeActivity(solo, translations).joinSecret.empty());
}

TEST_CASE("Discord fake: the player accepts to join a friend, at the next poll", "[discord]")
{
    testing::FakeSocialSdk sdk;
    std::string joined;
    sdk.presence.onJoin([&joined](const std::string &secret) { joined = secret; });
    sdk.presence.join("rtype1:7:rtype.example.com:4242");
    CHECK(joined.empty());
    sdk.poll();
    CHECK(joined == "rtype1:7:rtype.example.com:4242");
}
