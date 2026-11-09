// Tests of the colours of the auras around the ships, chosen on the Settings page and kept from one run of
// the client to the next.

#include "Settings/AuraSettings.hpp"
#include "Settings/NextValue.hpp"
#include "rtype/engine/Settings/Settings.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <ranges>
#include <string_view>
#include <vector>

using namespace rtype::client;

TEST_CASE("A first run gives the default colours to the auras", "[settings]")
{
    const rtype::engine::Settings settings;

    const AuraColors colors = readAuraColors(settings);

    CHECK(colors.own == AuraColors{}.own);
    CHECK(colors.allies == AuraColors{}.allies);
    CHECK(colors.enemies == AuraColors{}.enemies);
}

TEST_CASE("The colours of the auras kept are read again", "[settings]")
{
    rtype::engine::Settings settings;
    keepAuraColors(settings, {.own = "yellow", .allies = "off", .enemies = "purple"});

    const AuraColors colors = readAuraColors(settings);

    CHECK(colors.own == "yellow");
    CHECK(colors.allies == "off");
    CHECK(colors.enemies == "purple");
    CHECK(settings.getString(OwnAuraSettingsKey) == "yellow");
}

TEST_CASE("A colour of an aura that is not one keeps the default", "[settings]")
{
    rtype::engine::Settings settings;
    settings.setString(EnemyAuraSettingsKey, "magenta");

    CHECK(readAuraColors(settings).enemies == AuraColors{}.enemies);
}

TEST_CASE("An aura switched off has no colour", "[settings]")
{
    CHECK_FALSE(auraColor("off").has_value());
    CHECK_FALSE(auraColor("unknown").has_value());
    REQUIRE(auraColor("red").has_value());
    CHECK(auraColor("red")->a == 255);
}

TEST_CASE("The rows of the auras go through every colour and back", "[settings]")
{
    std::vector<std::string_view> names;
    for (const AuraColorChoice &choice : AuraColorChoices) {
        names.push_back(choice.name);
    }
    std::string_view name = names.front();
    for (std::size_t step = 0; step < names.size(); ++step) {
        name = nextValue(names, name);
    }
    CHECK(name == names.front());
    CHECK(previousValue(names, names.front()) == names.back());
    // The defaults are colours offered
    CHECK(std::ranges::find(names, AuraColors{}.own) != names.end());
}
