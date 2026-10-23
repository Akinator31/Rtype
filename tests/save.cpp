#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/save/Save/Save.hpp"

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
struct SaveDirectory {
    std::filesystem::path path;

    SaveDirectory()
    {
        static std::atomic<unsigned> next = 0;
        path = std::filesystem::temp_directory_path() /
               ("rtype_save_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                "_" + std::to_string(next++));
        std::filesystem::create_directories(path);
    }
    SaveDirectory(const SaveDirectory &) = delete;
    SaveDirectory &operator=(const SaveDirectory &) = delete;
    ~SaveDirectory()
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

TEST_CASE("A missing key has no value", "[save]")
{
    const rtype::save::Save save;

    REQUIRE_FALSE(save.hasKey("audio.volume"));
    REQUIRE_FALSE(save.getString("audio.volume"));
    REQUIRE_FALSE(save.getInt("audio.volume"));
}

TEST_CASE("Each type reads back the value it was set with", "[save]")
{
    rtype::save::Save save;

    save.setString("player.name", "Nova");
    save.setInt("audio.volume", -12);
    save.setFloat("video.scale", 1.25f);
    save.setBool("video.fullscreen", true);

    REQUIRE(save.getString("player.name") == "Nova");
    REQUIRE(save.getInt("audio.volume") == -12);
    REQUIRE(save.getFloat("video.scale") == 1.25f);
    REQUIRE(save.getBool("video.fullscreen") == true);
}

TEST_CASE("A value read as another type has no value", "[save]")
{
    rtype::save::Save save;

    save.setString("player.name", "Nova");
    save.setString("audio.volume", "12abc");

    REQUIRE_FALSE(save.getInt("player.name"));
    REQUIRE_FALSE(save.getBool("player.name"));
    REQUIRE_FALSE(save.getInt("audio.volume"));
    REQUIRE(save.getString("audio.volume") == "12abc");
}

TEST_CASE("Setting a key again replaces its value, removing it forgets it", "[save]")
{
    rtype::save::Save save;

    save.setInt("audio.volume", 40);
    save.setInt("audio.volume", 80);
    REQUIRE(save.getInt("audio.volume") == 80);

    save.removeKey("audio.volume");
    save.removeKey("unknown");
    REQUIRE_FALSE(save.hasKey("audio.volume"));
}

TEST_CASE("Values written to the file are read back by another save", "[save]")
{
    const SaveDirectory directory;
    const std::filesystem::path path = directory.path / "settings" / "save.txt";
    rtype::save::Save written(path);
    written.setInt("audio.volume", 80);
    written.setFloat("video.scale", 0.1f);
    written.setBool("video.fullscreen", false);
    written.setString("odd = key\\", "line one\nline two=\r\\end");

    written.writeFile();
    rtype::save::Save read(path);
    read.readFile();

    REQUIRE(read.getInt("audio.volume") == 80);
    REQUIRE(read.getFloat("video.scale") == 0.1f);
    REQUIRE(read.getBool("video.fullscreen") == false);
    REQUIRE(read.getString("odd = key\\") == "line one\nline two=\r\\end");
    REQUIRE_FALSE(std::filesystem::exists(directory.path / "settings" / "save.txt.tmp"));
}

TEST_CASE("The file has one line per value, sorted by key", "[save]")
{
    const SaveDirectory directory;
    rtype::save::Save save(directory.path / "save.txt");
    save.setInt("video.width", 1920);
    save.setInt("audio.volume", 80);

    save.writeFile();

    REQUIRE(readText(directory.path / "save.txt") == "audio.volume=80\nvideo.width=1920\n");
}

TEST_CASE("Reading the file replaces the values in memory", "[save]")
{
    const SaveDirectory directory;
    rtype::save::Save save(directory.path / "save.txt");
    save.setInt("audio.volume", 80);
    save.writeFile();
    save.setInt("audio.volume", 10);
    save.setBool("video.fullscreen", true);

    save.readFile();

    REQUIRE(save.getInt("audio.volume") == 80);
    REQUIRE_FALSE(save.hasKey("video.fullscreen"));
}

TEST_CASE("A missing file leaves no value, as on a first run", "[save]")
{
    const SaveDirectory directory;
    rtype::save::Save save(directory.path / "save.txt");
    save.setInt("audio.volume", 80);

    save.readFile();

    REQUIRE_FALSE(save.hasKey("audio.volume"));
}

TEST_CASE("Lines without '=' are ignored, Windows line ends are accepted", "[save]")
{
    const SaveDirectory directory;
    std::ofstream(directory.path / "save.txt", std::ios::binary) << "audio.volume=80\r\n\nbroken line\nname=\n";
    rtype::save::Save save(directory.path / "save.txt");

    save.readFile();

    REQUIRE(save.getInt("audio.volume") == 80);
    REQUIRE(save.getString("name") == "");
    REQUIRE_FALSE(save.hasKey("broken line"));
}

TEST_CASE("The engine owns a save without a file", "[save][engine]")
{
    rtype::engine::Engine engine;

    REQUIRE(engine.getSave().getPath().empty());
    engine.getSave().setInt("audio.volume", 80);
    REQUIRE(engine.getSave().getInt("audio.volume") == 80);
}
