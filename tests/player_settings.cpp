// Tests of the choices of the Settings page kept from one run of the client to the next: the language and the
// volumes and the colour filter. The audio is disabled, so that the tests play nothing.

#include "Settings/PlayerSettings.hpp"
#include "rtype/audio/AudioDevice.hpp"
#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/engine/Settings/Settings.hpp"
#include "rtype/engine/ThreadManager.hpp"
#include "rtype/localization/Localization.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

using namespace rtype::client;

namespace
{

/// A client with its settings in a file of its own, and the translations in English and French, all removed
/// after the test: a directory per test, since CTest runs them at once.
struct Client {
    std::filesystem::path directory;
    rtype::engine::Settings settings;
    rtype::audio::AudioDevice device{{.enabled = false}};
    rtype::engine::ThreadManager threads;
    rtype::audio::AudioPlayer audio{device, ".", threads.getAssetsWorker()};
    rtype::localization::Localization translations{"en"};

    explicit Client(const std::string &test)
        : directory(std::filesystem::temp_directory_path() / ("rtype_tests_player_settings_" + test)),
          settings(directory / "settings.txt")
    {
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        std::ofstream(directory / "en.lang") << "menu.play=Play\n";
        std::ofstream(directory / "fr.lang") << "menu.play=Jouer\n";
        translations.load("en", directory / "en.lang");
        translations.load("fr", directory / "fr.lang");
    }
    ~Client()
    {
        threads.stopAll();
        std::filesystem::remove_all(directory);
    }

    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;
};

} // namespace

TEST_CASE("A first run keeps the default language and volumes", "[settings]")
{
    Client client("first_run");
    client.audio.setMusicVolume(0.2f);
    client.audio.setEffectsVolume(0.3f);

    applyPlayerSettings(client.settings, client.translations, client.audio);

    CHECK(client.translations.language() == "en");
    CHECK(client.audio.musicVolume() == DefaultMusicVolume);
    CHECK(client.audio.effectsVolume() == DefaultEffectsVolume);
}

TEST_CASE("The language and the volumes chosen are found again at the next run", "[settings]")
{
    Client client("next_run");
    client.translations.setLanguage("fr");
    client.audio.setMusicVolume(0.25f);
    client.audio.setEffectsVolume(0.75f);
    writePlayerSettings(client.settings, client.translations, client.audio, rtype::render::ColorFilter::None);

    // The next run: a new client reads the file the last one wrote
    rtype::engine::Settings next(client.settings.getPath());
    next.readFile();
    client.translations.setLanguage("en");
    client.audio.setMusicVolume(1.0f);
    client.audio.setEffectsVolume(1.0f);
    applyPlayerSettings(next, client.translations, client.audio);

    CHECK(client.translations.language() == "fr");
    CHECK(client.audio.musicVolume() == 0.25f);
    CHECK(client.audio.effectsVolume() == 0.75f);
}

TEST_CASE("Writing the settings keeps the values of the other parts of the client", "[settings]")
{
    Client client("other_values");
    client.settings.setString("ship.equipped", "VX-03");

    writePlayerSettings(client.settings, client.translations, client.audio, rtype::render::ColorFilter::None);
    rtype::engine::Settings read(client.settings.getPath());
    read.readFile();

    CHECK(read.getString("ship.equipped") == "VX-03");
    CHECK(read.getString(LanguageSettingsKey) == "en");
}

TEST_CASE("A wrong value in the settings leaves the default", "[settings]")
{
    Client client("wrong_values");
    client.settings.setString(LanguageSettingsKey, "klingon");
    client.settings.setString(MusicVolumeSettingsKey, "loud");
    client.settings.setString(EffectsVolumeSettingsKey, "nan");

    applyPlayerSettings(client.settings, client.translations, client.audio);

    CHECK(client.translations.language() == "en");
    CHECK(client.audio.musicVolume() == DefaultMusicVolume);
    CHECK(client.audio.effectsVolume() == DefaultEffectsVolume);
}

TEST_CASE("A volume out of range in the settings is clamped", "[settings]")
{
    Client client("out_of_range");
    client.settings.setFloat(MusicVolumeSettingsKey, 3.0f);
    client.settings.setFloat(EffectsVolumeSettingsKey, -1.0f);

    applyPlayerSettings(client.settings, client.translations, client.audio);

    CHECK(client.audio.musicVolume() == 1.0f);
    CHECK(client.audio.effectsVolume() == 0.0f);
}

TEST_CASE("The colour filter chosen is found again at the next run", "[settings]")
{
    Client client("color_filter");
    writePlayerSettings(client.settings, client.translations, client.audio,
                        rtype::render::ColorFilter::Deuteranopia);

    rtype::engine::Settings next(client.settings.getPath());
    next.readFile();

    CHECK(next.getString(ColorFilterSettingsKey) == "deuteranopia");
    CHECK(readColorFilter(next) == rtype::render::ColorFilter::Deuteranopia);
}

TEST_CASE("No colour filter, or a wrong one, in the settings applies none", "[settings]")
{
    Client client("wrong_color_filter");
    CHECK(readColorFilter(client.settings) == rtype::render::ColorFilter::None);

    client.settings.setString(ColorFilterSettingsKey, "sepia");
    CHECK(readColorFilter(client.settings) == rtype::render::ColorFilter::None);
    // The simulations are for the team, not kept for a player
    client.settings.setString(ColorFilterSettingsKey, "simulate_protanopia");
    CHECK(readColorFilter(client.settings) == rtype::render::ColorFilter::None);
}

TEST_CASE("The colour filter button goes through the filters of the players and around", "[settings]")
{
    using rtype::render::ColorFilter;
    ColorFilter filter = ColorFilter::None;
    for (const ColorFilter expected :
         {ColorFilter::Protanopia, ColorFilter::Deuteranopia, ColorFilter::Tritanopia,
          ColorFilter::HighContrast, ColorFilter::None}) {
        filter = nextColorFilter(filter);
        CHECK(filter == expected);
    }
    // From a simulation, set by the team, the button starts again from the first
    CHECK(nextColorFilter(ColorFilter::SimulateTritanopia) == ColorFilter::None);
}

TEST_CASE("Every colour filter of the players has a name in both languages", "[settings]")
{
    for (const auto &language : {"en", "fr"}) {
        rtype::localization::Localization translations(language);
        translations.load(language,
                          std::filesystem::path(RTYPE_TEST_LOCALES) / (std::string(language) + ".lang"));
        for (const rtype::render::ColorFilter filter : PlayerColorFilters) {
            const std::string key = "settings.filter." + std::string(colorFilterSettingName(filter));
            CHECK(translations.text(key) != key);
        }
    }
}
