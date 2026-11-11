// Tests of the activity of the player in Discord (apps/client/src/Discord/DiscordPresence.hpp): what each
// place of the game says, in English and in French, and the tracker that updates the presence on the changes
// only, spaced, against the fake Social SDK (docs/architecture/client-online.md#rich-presence).

#include "Discord/DiscordPresence.hpp"
#include "FakeSocialSdk.hpp"
#include "rtype/engine/Settings/Settings.hpp"
#include "rtype/localization/Localization.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <string>

using rtype::client::PlayerActivity;
using rtype::client::PresenceTracker;
using Place = PlayerActivity::Place;

namespace
{

rtype::localization::Localization loadTranslations(const std::string &language)
{
    rtype::localization::Localization translations(language);
    translations.load(language, std::filesystem::path(RTYPE_TEST_LOCALES) / (language + ".lang"));
    return translations;
}

PlayerActivity at(Place place)
{
    PlayerActivity activity;
    activity.place = place;
    activity.shipCode = "VX-03";
    activity.shipNameKey = "ship.vx03";
    return activity;
}

const std::chrono::system_clock::time_point started{std::chrono::seconds(1'700'000'000)};

} // namespace

TEST_CASE("Discord presence: each place of the game has its texts", "[discord]")
{
    const rtype::localization::Localization translations = loadTranslations("fr");

    const rtype::discord::Activity hangar = describeActivity(at(Place::Hangar), translations);
    CHECK(hangar.details == "Dans le hangar");
    CHECK(hangar.state.empty());
    CHECK_FALSE(hangar.party);
    CHECK_FALSE(hangar.startedAt);
    CHECK(hangar.largeImage == "logo");
    CHECK(hangar.largeText == "R-Type");
    // The ship in a game only
    CHECK(hangar.smallImage.empty());
    CHECK(hangar.smallText.empty());

    const rtype::discord::Activity online = describeActivity(at(Place::Online), translations);
    CHECK(online.details == "En ligne");
    CHECK(online.state == "Choisit une partie");

    PlayerActivity solo = at(Place::Solo);
    solo.startedAt = started;
    solo.waveCount = 12;
    CHECK(describeActivity(solo, translations).state.empty());
    solo.wave = 7;
    const rtype::discord::Activity soloShown = describeActivity(solo, translations);
    CHECK(soloShown.details == "Solo");
    CHECK(soloShown.state == "Vague 7/12");
    CHECK(soloShown.startedAt == started);
    CHECK(soloShown.largeImage == "logo");
    CHECK(soloShown.smallImage == "vx-03");
    CHECK(soloShown.smallText.starts_with("VX-03 · "));
    CHECK_FALSE(soloShown.party);

    PlayerActivity game = at(Place::OnlineGame);
    game.wave = 31;
    game.players = 3;
    game.maxPlayers = 4;
    game.startedAt = started;
    const rtype::discord::Activity gameShown = describeActivity(game, translations);
    CHECK(gameShown.details == "Partie en ligne");
    CHECK(gameShown.state == "Vague 31");
    CHECK(gameShown.party == rtype::discord::Party{.size = 3, .max = 4});
    CHECK(gameShown.startedAt == started);

    const rtype::discord::Activity results = describeActivity(at(Place::OnlineResults), translations);
    CHECK(results.details == "Partie en ligne");
    CHECK(results.state == "Partie terminée");
    PlayerActivity cleared = at(Place::OnlineResults);
    cleared.wavesCleared = 31;
    CHECK(describeActivity(cleared, translations).state == "Partie terminée · 31 vagues franchies");
    cleared.wavesCleared = 1;
    CHECK(describeActivity(cleared, translations).state == "Partie terminée · 1 vague franchie");
}

TEST_CASE("Discord presence: each screen out of a game says where the player is", "[discord]")
{
    const rtype::localization::Localization translations = loadTranslations("en");
    const auto shown = [&translations](Place place) {
        const rtype::discord::Activity activity = describeActivity(at(place), translations);
        return activity.details + " / " + activity.state;
    };
    CHECK(shown(Place::Ships) == "In the hangar / Choosing a ship");
    CHECK(shown(Place::Settings) == "In the settings / ");
    CHECK(shown(Place::Lore) == "In the hangar / Reading the lore");
    CHECK(shown(Place::Quests) == "In the hangar / Checking the quests");
    CHECK(shown(Place::Servers) == "Online / Choosing a server");
    CHECK(shown(Place::SigningIn) == "Online / Signing in");
    CHECK(shown(Place::Account) == "Online / Managing the account");
    CHECK(shown(Place::Online) == "Online / Choosing a game");
}

TEST_CASE("Discord presence: a game tells its boss, its pause, its final battle and its end", "[discord]")
{
    const rtype::localization::Localization translations = loadTranslations("en");
    PlayerActivity solo = at(Place::Solo);
    solo.wave = 12;
    solo.waveCount = 12;
    solo.boss = true;
    CHECK(describeActivity(solo, translations).state == "Wave 12/12 · Boss fight");
    solo.paused = true;
    CHECK(describeActivity(solo, translations).state == "Wave 12/12 · Paused");
    solo.finalBattle = true;
    CHECK(describeActivity(solo, translations).state == "Wave 12/12 · Final battle");
    solo.outcome = rtype::game::GameOutcome::Victory;
    CHECK(describeActivity(solo, translations).state == "Wave 12/12 · Victory");

    PlayerActivity online = at(Place::OnlineGame);
    online.boss = true;
    CHECK(describeActivity(online, translations).state == "Boss fight");
    online.wave = 9;
    online.outcome = rtype::game::GameOutcome::Defeat;
    CHECK(describeActivity(online, translations).state == "Wave 9 · Defeat");
}

TEST_CASE("Discord presence: every text is in English and in French", "[discord]")
{
    for (const std::string language : {"en", "fr"}) {
        CAPTURE(language);
        const rtype::localization::Localization translations = loadTranslations(language);
        for (const char *key : {"presence.hangar",
                                "presence.online",
                                "presence.choosing_game",
                                "presence.solo",
                                "presence.online_game",
                                "presence.wave",
                                "presence.game_over",
                                "presence.choosing_ship",
                                "presence.settings",
                                "presence.lore",
                                "presence.quests",
                                "presence.choosing_server",
                                "presence.signing_in",
                                "presence.account",
                                "presence.boss",
                                "presence.final_battle",
                                "presence.paused",
                                "presence.victory",
                                "presence.defeat",
                                "presence.wave_cleared",
                                "presence.waves_cleared",
                                "invite.invalid",
                                "invite.in_game",
                                "invite.joining",
                                "settings.discord_activity",
                                "settings.group.discord",
                                "settings.hint.discord_activity"}) {
            CAPTURE(key);
            CHECK(translations.text(key) != key);
        }
    }
}

TEST_CASE("Discord presence: the tracker sends the changes only, spaced by the limit of Discord", "[discord]")
{
    const rtype::localization::Localization translations = loadTranslations("en");
    testing::FakeSocialSdk sdk;
    PresenceTracker tracker(sdk.presence, translations);
    PresenceTracker::Clock::time_point now{};

    tracker.update(at(Place::Hangar), now);
    REQUIRE(sdk.presence.updates.size() == 1);
    CHECK(sdk.presence.shown->details == "In the hangar");

    // The same every frame: nothing more
    for (int frame = 0; frame < 300; ++frame) {
        now += std::chrono::milliseconds(16);
        tracker.update(at(Place::Hangar), now);
    }
    CHECK(sdk.presence.updates.size() == 1);

    // Two changes within the limit: the last one, once it passed
    const auto changed = now;
    tracker.update(at(Place::Online), now);
    CHECK(sdk.presence.updates.size() == 2);
    PlayerActivity solo = at(Place::Solo);
    solo.wave = 1;
    tracker.update(solo, now + std::chrono::seconds(1));
    solo.wave = 2;
    tracker.update(solo, now + std::chrono::seconds(2));
    CHECK(sdk.presence.updates.size() == 2);
    tracker.update(solo, changed + PresenceTracker::MinInterval);
    REQUIRE(sdk.presence.updates.size() == 3);
    CHECK(sdk.presence.shown->state == "Wave 2");
}

TEST_CASE("Discord presence: turned off, the activity is cleared and nothing is sent", "[discord]")
{
    const rtype::localization::Localization translations = loadTranslations("en");
    testing::FakeSocialSdk sdk;
    PresenceTracker tracker(sdk.presence, translations);
    PresenceTracker::Clock::time_point now{};

    tracker.update(at(Place::Hangar), now);
    REQUIRE(sdk.presence.shown);
    tracker.setEnabled(false);
    CHECK_FALSE(tracker.isEnabled());
    CHECK_FALSE(sdk.presence.shown);
    tracker.update(at(Place::Online), now + std::chrono::minutes(1));
    CHECK_FALSE(sdk.presence.shown);
    CHECK(sdk.presence.updates.size() == 1);

    // On again: shown at the next update
    tracker.setEnabled(true);
    tracker.update(at(Place::Online), now + std::chrono::minutes(2));
    REQUIRE(sdk.presence.shown);
    CHECK(sdk.presence.shown->details == "Online");

    // Off from the start: nothing ever reaches Discord
    testing::FakeSocialSdk other;
    PresenceTracker off(other.presence, translations, false);
    off.update(at(Place::Hangar), now);
    CHECK(other.presence.updates.empty());
}

TEST_CASE("Discord presence: an activity Discord refused is sent again until it takes it", "[discord]")
{
    const rtype::localization::Localization translations = loadTranslations("en");
    testing::FakeSocialSdk sdk;
    PresenceTracker tracker(sdk.presence, translations);
    PresenceTracker::Clock::time_point now{};

    // Discord not running yet: refused, at the next poll
    sdk.presence.refusing = true;
    tracker.update(at(Place::Hangar), now);
    REQUIRE(sdk.presence.updates.size() == 1);
    sdk.poll();
    now += PresenceTracker::MinInterval;
    tracker.update(at(Place::Hangar), now);
    CHECK(sdk.presence.updates.size() == 1);

    // Started: taken at the next try, once the retry interval passed
    sdk.presence.refusing = false;
    now += PresenceTracker::RetryInterval;
    tracker.update(at(Place::Hangar), now);
    REQUIRE(sdk.presence.updates.size() == 2);
    sdk.poll();
    REQUIRE(sdk.presence.shown);
    CHECK(sdk.presence.shown->details == "In the hangar");
    now += PresenceTracker::RetryInterval;
    tracker.update(at(Place::Hangar), now);
    CHECK(sdk.presence.updates.size() == 2);
}

TEST_CASE("Discord presence: lost by a connection to Discord, the activity is sent again", "[discord]")
{
    const rtype::localization::Localization translations = loadTranslations("en");
    testing::FakeSocialSdk sdk;
    PresenceTracker tracker(sdk.presence, translations);
    sdk.connection.onStatusChanged([&tracker](rtype::discord::ConnectionStatus status) {
        if (status != rtype::discord::ConnectionStatus::Connecting) {
            tracker.resend();
        }
    });
    PresenceTracker::Clock::time_point now{};

    tracker.update(at(Place::Online), now);
    sdk.poll();
    REQUIRE(sdk.presence.shown);

    // Connect() clears it: once ready, shown again at the next frame, whatever the interval
    sdk.connection.connect("access");
    sdk.presence.lose();
    sdk.connection.becomeReady();
    sdk.poll();
    now += std::chrono::seconds(1);
    tracker.update(at(Place::Online), now);
    REQUIRE(sdk.presence.shown);
    CHECK(sdk.presence.shown->state == "Choosing a game");

    // Disconnected, a failed connection included: shown again
    sdk.presence.lose();
    sdk.connection.lose();
    sdk.poll();
    now += PresenceTracker::MinInterval;
    tracker.update(at(Place::Online), now);
    REQUIRE(sdk.presence.shown);
    CHECK(sdk.presence.updates.size() == 3);
}

TEST_CASE("Discord presence: the language changed, the texts follow", "[discord]")
{
    rtype::localization::Localization translations = loadTranslations("en");
    translations.load("fr", std::filesystem::path(RTYPE_TEST_LOCALES) / "fr.lang");
    testing::FakeSocialSdk sdk;
    PresenceTracker tracker(sdk.presence, translations);
    PresenceTracker::Clock::time_point now{};

    tracker.update(at(Place::Hangar), now);
    translations.setLanguage("fr");
    tracker.update(at(Place::Hangar), now + PresenceTracker::MinInterval);
    REQUIRE(sdk.presence.updates.size() == 2);
    CHECK(sdk.presence.shown->details == "Dans le hangar");
}

TEST_CASE("Discord presence: the settings keep the choice and the application", "[discord]")
{
    rtype::engine::Settings settings;
    CHECK(rtype::client::readDiscordActivity(settings));
    settings.setBool(rtype::client::DiscordActivitySettingsKey, false);
    CHECK_FALSE(rtype::client::readDiscordActivity(settings));

    CHECK(rtype::client::readDiscordApplication(settings) == rtype::client::defaultDiscordApplication());
    settings.setString(rtype::client::DiscordApplicationSettingsKey, "1234567890123456789");
    CHECK(rtype::client::readDiscordApplication(settings) == 1234567890123456789ULL);
    settings.setString(rtype::client::DiscordApplicationSettingsKey, "not a number");
    CHECK(rtype::client::readDiscordApplication(settings) == 0);
}
