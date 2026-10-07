// The audio library is tested with audio disabled: the tests give the same result on every computer, with or
// without a sound card, and play nothing.

#include "rtype/audio/AudioDevice.hpp"
#include "rtype/audio/AudioPlayer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>

using namespace rtype::audio;

namespace
{

// A directory of fake assets, removed at the end of the test. Audio is disabled, so the files are only
// checked to exist: their content does not matter.
class FakeAssets
{
private:
    std::filesystem::path _directory = std::filesystem::temp_directory_path() / "rtype_audio_tests";

public:
    FakeAssets()
    {
        std::filesystem::create_directories(_directory / "sounds");
        std::ofstream(_directory / "sounds" / "shot.wav") << "fake";
        std::ofstream(_directory / "sounds" / "music.ogg") << "fake";
    }
    ~FakeAssets() { std::filesystem::remove_all(_directory); }

    FakeAssets(const FakeAssets &) = delete;
    FakeAssets &operator=(const FakeAssets &) = delete;

    [[nodiscard]] const std::filesystem::path &directory() const { return _directory; }
};

} // namespace

TEST_CASE("A disabled audio device is not available", "[audio]")
{
    const AudioDevice device({.enabled = false});

    REQUIRE_FALSE(device.isAvailable());
}

TEST_CASE("Only one audio device can exist at a time", "[audio]")
{
    const AudioDevice device({.enabled = false});

    REQUIRE_THROWS_AS(AudioDevice({.enabled = false}), std::logic_error);
}

TEST_CASE("Volumes are clamped between 0 and 1", "[audio]")
{
    AudioDevice device({.enabled = false});
    AudioPlayer player(device, ".");

    device.setMasterVolume(2.0f);
    player.setMusicVolume(-1.0f);
    player.setEffectsVolume(0.5f);

    REQUIRE(device.masterVolume() == 1.0f);
    REQUIRE(player.musicVolume() == 0.0f);
    REQUIRE(player.effectsVolume() == 0.5f);
}

TEST_CASE("Without audio, sounds are loaded once and play silently", "[audio]")
{
    const FakeAssets assets;
    const AudioDevice device({.enabled = false});
    AudioPlayer player(device, assets.directory());

    const SoundId first = player.loadSound("sounds/shot.wav");
    const SoundId second = player.loadSound("sounds/shot.wav");
    REQUIRE(first == second);

    REQUIRE_NOTHROW(player.play(first, {.volume = 0.5f, .pitch = 1.2f, .pan = -1.0f}));
}

TEST_CASE("A missing file is reported even without audio", "[audio]")
{
    const FakeAssets assets;
    const AudioDevice device({.enabled = false});
    AudioPlayer player(device, assets.directory());

    REQUIRE_THROWS_AS(player.loadSound("sounds/missing.wav"), std::runtime_error);
    REQUIRE_THROWS_AS(player.loadMusic("music/missing.ogg"), std::runtime_error);
}

TEST_CASE("Without audio, the music state follows the calls", "[audio]")
{
    const FakeAssets assets;
    const AudioDevice device({.enabled = false});
    AudioPlayer player(device, assets.directory());
    const MusicId music = player.loadMusic("sounds/music.ogg");

    REQUIRE_FALSE(player.isMusicPlaying());
    player.playMusic(music);
    player.update();
    REQUIRE(player.isMusicPlaying());

    player.pauseMusic();
    REQUIRE_FALSE(player.isMusicPlaying());
    player.resumeMusic();
    REQUIRE(player.isMusicPlaying());

    player.stopMusic();
    REQUIRE_FALSE(player.isMusicPlaying());
}
