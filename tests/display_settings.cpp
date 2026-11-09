// Tests of the screen and the resolution chosen on the Settings page: the resolutions offered on a screen,
// and how they are kept in the settings. Moving the window itself needs a screen, and is not tested here.

#include "Settings/DisplaySettings.hpp"
#include "Settings/NextValue.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using rtype::client::Fullscreen;
using rtype::client::nextValue;
using rtype::client::parseResolution;
using rtype::client::previousValue;
using rtype::client::Resolution;
using rtype::client::resolutionSettingName;
using rtype::client::resolutionsFor;
using rtype::client::stepValue;

TEST_CASE("A screen offers fullscreen, then the window sizes smaller than it", "[settings]")
{
    CHECK(resolutionsFor({"", 1920, 1080}) == std::vector<Resolution>{Fullscreen, {1280, 720}, {1600, 900}});
    CHECK(resolutionsFor({"", 2560, 1440}) ==
          std::vector<Resolution>{Fullscreen, {1280, 720}, {1600, 900}, {1920, 1080}});
    // A screen too small for any window size only offers fullscreen
    CHECK(resolutionsFor({"", 1024, 768}) == std::vector<Resolution>{Fullscreen});
}

TEST_CASE("The resolution button goes through the resolutions of the screen and around", "[settings]")
{
    const std::vector<Resolution> offered = resolutionsFor({"", 1920, 1080});

    CHECK(nextValue(offered, Fullscreen) == Resolution{1280, 720});
    CHECK(nextValue(offered, Resolution{1600, 900}) == Fullscreen);
    // A window resized by hand, or too large for a new screen, starts again from fullscreen
    CHECK(nextValue(offered, Resolution{1234, 567}) == Fullscreen);
}

TEST_CASE("Left goes back through the resolutions, and around from the first", "[settings]")
{
    const std::vector<Resolution> offered = resolutionsFor({"", 1920, 1080});

    CHECK(previousValue(offered, Resolution{1600, 900}) == Resolution{1280, 720});
    CHECK(previousValue(offered, Fullscreen) == Resolution{1600, 900});
    CHECK(previousValue(offered, Resolution{1234, 567}) == Fullscreen);
    CHECK(stepValue(offered, Fullscreen, -1) == Resolution{1600, 900});
    CHECK(stepValue(offered, Fullscreen, 1) == Resolution{1280, 720});
}

TEST_CASE("A resolution is kept as text and read back", "[settings]")
{
    CHECK(resolutionSettingName(Fullscreen) == "fullscreen");
    CHECK(resolutionSettingName({1600, 900}) == "1600x900");
    CHECK(parseResolution("fullscreen") == Fullscreen);
    CHECK(parseResolution("1600x900") == Resolution{1600, 900});
}

TEST_CASE("A wrong resolution in the settings is not read", "[settings]")
{
    for (const char *text : {"", "big", "1600", "1600x", "x900", "1600x900px", "-1600x900", "0x0"}) {
        CAPTURE(text);
        CHECK_FALSE(parseResolution(text));
    }
}

TEST_CASE("The frame rate kept is read back, the default for none or a wrong one", "[settings]")
{
    rtype::engine::Settings settings;
    CHECK(rtype::client::readFpsLimit(settings) == rtype::client::DefaultFpsLimit);

    settings.setInt(rtype::client::FpsLimitSettingsKey, 144);
    CHECK(rtype::client::readFpsLimit(settings) == 144);
    settings.setInt(rtype::client::FpsLimitSettingsKey, rtype::client::UnlimitedFps);
    CHECK(rtype::client::readFpsLimit(settings) == rtype::client::UnlimitedFps);
    settings.setInt(rtype::client::FpsLimitSettingsKey, -30);
    CHECK(rtype::client::readFpsLimit(settings) == rtype::client::DefaultFpsLimit);
    settings.setString(rtype::client::FpsLimitSettingsKey, "fast");
    CHECK(rtype::client::readFpsLimit(settings) == rtype::client::DefaultFpsLimit);
}

TEST_CASE("The frame rate button goes from 60 to unlimited and around", "[settings]")
{
    using rtype::client::FpsLimits;
    CHECK(nextValue(FpsLimits, 60) == 120);
    CHECK(nextValue(FpsLimits, 240) == rtype::client::UnlimitedFps);
    CHECK(nextValue(FpsLimits, rtype::client::UnlimitedFps) == 60);
    // A limit set elsewhere starts again from the first
    CHECK(nextValue(FpsLimits, 75) == 60);
}
