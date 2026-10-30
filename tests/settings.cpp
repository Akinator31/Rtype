#include "rtype/engine/Settings/Settings.hpp"

#include "rtype/engine/Engine/Engine.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{

/// A directory of its own for each test, removed at the end.
struct SettingsDirectory {
    std::filesystem::path path;

    SettingsDirectory()
    {
        static std::atomic<unsigned> next = 0;
        path =
            std::filesystem::temp_directory_path() /
            ("rtype_settings_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
             "_" + std::to_string(next++));
        std::filesystem::create_directories(path);
    }
    SettingsDirectory(const SettingsDirectory &) = delete;
    SettingsDirectory &operator=(const SettingsDirectory &) = delete;
    ~SettingsDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

std::string readText(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

} // namespace

TEST_CASE("A missing key has no value", "[settings]")
{
    const rtype::engine::Settings settings;

    REQUIRE_FALSE(settings.hasKey("audio.volume"));
    REQUIRE_FALSE(settings.getString("audio.volume"));
    REQUIRE_FALSE(settings.getInt("audio.volume"));
}

TEST_CASE("Each type reads back the value it was set with", "[settings]")
{
    rtype::engine::Settings settings;

    settings.setString("player.name", "Nova");
    settings.setInt("audio.volume", -12);
    settings.setFloat("video.scale", 1.25f);
    settings.setBool("video.fullscreen", true);

    REQUIRE(settings.getString("player.name") == "Nova");
    REQUIRE(settings.getInt("audio.volume") == -12);
    REQUIRE(settings.getFloat("video.scale") == 1.25f);
    REQUIRE(settings.getBool("video.fullscreen") == true);
}

TEST_CASE("A value read as another type has no value", "[settings]")
{
    rtype::engine::Settings settings;

    settings.setString("player.name", "Nova");
    settings.setString("audio.volume", "12abc");

    REQUIRE_FALSE(settings.getInt("player.name"));
    REQUIRE_FALSE(settings.getBool("player.name"));
    REQUIRE_FALSE(settings.getInt("audio.volume"));
    REQUIRE(settings.getString("audio.volume") == "12abc");
}

TEST_CASE("Setting a key again replaces its value, removing it forgets it", "[settings]")
{
    rtype::engine::Settings settings;

    settings.setInt("audio.volume", 40);
    settings.setInt("audio.volume", 80);
    REQUIRE(settings.getInt("audio.volume") == 80);

    settings.removeKey("audio.volume");
    settings.removeKey("unknown");
    REQUIRE_FALSE(settings.hasKey("audio.volume"));
}

TEST_CASE("Values written to the file are read back by other settings", "[settings]")
{
    const SettingsDirectory directory;
    const std::filesystem::path path = directory.path / "settings" / "settings.txt";
    rtype::engine::Settings written(path);
    written.setInt("audio.volume", 80);
    written.setFloat("video.scale", 0.1f);
    written.setBool("video.fullscreen", false);
    written.setString("odd = key\\", "line one\nline two=\r\\end");

    written.writeFile();
    rtype::engine::Settings read(path);
    read.readFile();

    REQUIRE(read.getInt("audio.volume") == 80);
    REQUIRE(read.getFloat("video.scale") == 0.1f);
    REQUIRE(read.getBool("video.fullscreen") == false);
    REQUIRE(read.getString("odd = key\\") == "line one\nline two=\r\\end");
    REQUIRE_FALSE(std::filesystem::exists(directory.path / "settings" / "settings.txt.tmp"));
}

TEST_CASE("The file has one line per value, sorted by key", "[settings]")
{
    const SettingsDirectory directory;
    rtype::engine::Settings settings(directory.path / "settings.txt");
    settings.setInt("video.width", 1920);
    settings.setInt("audio.volume", 80);

    settings.writeFile();

    REQUIRE(readText(directory.path / "settings.txt") == "audio.volume=80\nvideo.width=1920\n");
}

#ifndef _WIN32
TEST_CASE("The file is readable by its owner only: it holds session tokens", "[settings]")
{
    const SettingsDirectory directory;
    const std::filesystem::path path = directory.path / "settings.txt";
    // A file left readable by everyone, by an older client, is written again for its owner only
    std::ofstream(path) << "audio.volume=80\n";
    std::filesystem::permissions(path, std::filesystem::perms::all);
    rtype::engine::Settings settings(path);
    settings.setString("servers.session.127.0.0.1:4242", "secret");

    settings.writeFile();

    using std::filesystem::perms;
    CHECK(std::filesystem::status(path).permissions() == (perms::owner_read | perms::owner_write));
}
#endif

TEST_CASE("Reading the file replaces the values in memory", "[settings]")
{
    const SettingsDirectory directory;
    rtype::engine::Settings settings(directory.path / "settings.txt");
    settings.setInt("audio.volume", 80);
    settings.writeFile();
    settings.setInt("audio.volume", 10);
    settings.setBool("video.fullscreen", true);

    settings.readFile();

    REQUIRE(settings.getInt("audio.volume") == 80);
    REQUIRE_FALSE(settings.hasKey("video.fullscreen"));
}

TEST_CASE("A missing file leaves no value, as on a first run", "[settings]")
{
    const SettingsDirectory directory;
    rtype::engine::Settings settings(directory.path / "settings.txt");
    settings.setInt("audio.volume", 80);

    settings.readFile();

    REQUIRE_FALSE(settings.hasKey("audio.volume"));
}

TEST_CASE("Lines without '=' are ignored, Windows line ends are accepted", "[settings]")
{
    const SettingsDirectory directory;
    std::ofstream(directory.path / "settings.txt", std::ios::binary)
        << "audio.volume=80\r\n\nbroken line\nname=\n";
    rtype::engine::Settings settings(directory.path / "settings.txt");

    settings.readFile();

    REQUIRE(settings.getInt("audio.volume") == 80);
    REQUIRE(settings.getString("name") == "");
    REQUIRE_FALSE(settings.hasKey("broken line"));
}

TEST_CASE("The engine owns settings without a file", "[settings][engine]")
{
    rtype::engine::Engine engine;

    REQUIRE(engine.getSettings().getPath().empty());
    engine.getSettings().setInt("audio.volume", 80);
    REQUIRE(engine.getSettings().getInt("audio.volume") == 80);
}
