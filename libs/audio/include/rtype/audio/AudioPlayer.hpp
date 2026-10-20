#pragma once

#include "rtype/core/Handle.hpp"

#include <filesystem>
#include <future>
#include <memory>
#include <string>

namespace rtype::core
{
class BackgroundLoader;
}

namespace rtype::audio
{

class AudioDevice;

/// Handle of a sound effect loaded by the AudioPlayer.
using SoundId = core::Handle<struct SoundTag>;
/// Handle of a music loaded by the AudioPlayer.
using MusicId = core::Handle<struct MusicTag>;

/// How a sound effect is played.
struct PlaySettings {
    /// Volume of this sound, between 0 and 1, multiplied by the effects volume.
    float volume = 1.0f;
    /// Speed and pitch: 1 is the original sound, 2 is one octave higher and twice as fast. Small random
    /// variations (0.9 to 1.1) keep a repeated sound from being tiresome.
    float pitch = 1.0f;
    /// Position from left to right: -1 is fully left, 0 is centred, 1 is fully right.
    float pan = 0.0f;
};

/// Loads and plays the sound effects and the music of the game.
///
/// **Sound effects** (shots, explosions) are short and fully loaded in memory. The same sound can be played
/// several times at once: each sound has a few voices, so that fast shots do not cut each other.
///
/// **Music** is long, so it is streamed from its file while it plays: call update() once per frame to keep
/// the stream fed. Only one music plays at a time.
///
/// Each file is loaded once, like textures in the render library. The player needs an AudioDevice that
/// outlives it; if the device is not available, everything still works but nothing is heard.
/// @code
/// AudioDevice device;
/// AudioPlayer audio(device, applicationDirectory() / "assets");
/// const SoundId shot = audio.loadSound("sounds/shot.wav");
/// const MusicId stage = audio.loadMusic("music/stage1.ogg");
///
/// audio.playMusic(stage);
/// while (...) {
///     audio.update();
///     if (shooting) { audio.play(shot); }
/// }
/// @endcode
/// Supported formats: WAV, OGG, MP3, QOA, and the XM and MOD tracker formats. FLAC is not supported.
class AudioPlayer
{
private:
    struct Impl;
    std::unique_ptr<Impl> _impl;

public:
    /// Creates a player, with nothing loaded.
    /// @param device The audio output. It must outlive the player.
    /// @param assetsDirectory Directory that the paths given to the load functions are relative to.
    AudioPlayer(const AudioDevice &device, std::filesystem::path assetsDirectory,
                std::shared_ptr<core::BackgroundLoader> loader = {});
    ~AudioPlayer();

    AudioPlayer(const AudioPlayer &) = delete;
    AudioPlayer &operator=(const AudioPlayer &) = delete;
    AudioPlayer(AudioPlayer &&) noexcept;
    AudioPlayer &operator=(AudioPlayer &&) noexcept;

    /// Loads a sound effect, unless it is already loaded.
    /// @param path Path of the file, relative to the assets directory, for instance "sounds/shot.wav".
    /// @throws std::runtime_error if the file is missing or is not a supported sound.
    [[nodiscard]] SoundId loadSound(const std::string &path);
    /// Prepares a music to be streamed, unless it is already loaded.
    /// @param path Path of the file, relative to the assets directory, for instance "music/stage1.ogg".
    /// @throws std::runtime_error if the file is missing or is not a supported music.
    [[nodiscard]] MusicId loadMusic(const std::string &path);

    [[nodiscard]] std::shared_future<SoundId> loadSoundAsync(const std::string &path);
    [[nodiscard]] std::shared_future<MusicId> loadMusicAsync(const std::string &path);
    void finishPendingLoads();

    /// Plays a sound effect from its start. If all its voices are busy, the oldest one is restarted.
    void play(SoundId sound, const PlaySettings &settings = {});

    /// Plays a music from its start, replacing the current one.
    /// @param loop Whether to start again at the end.
    void playMusic(MusicId music, bool loop = true);
    /// Stops the music.
    void stopMusic();
    /// Pauses the music, to resume it later where it stopped.
    void pauseMusic();
    /// Resumes a paused music.
    void resumeMusic();
    /// Tells whether a music is playing (not stopped nor paused).
    [[nodiscard]] bool isMusicPlaying() const;

    /// Feeds the music stream. Call it once per frame: otherwise the music stutters, then stops.
    void update();

    /// Sets the volume of the music, between 0 and 1. Clamped to this range. Applies to the current music
    /// immediately.
    void setMusicVolume(float volume);
    /// Volume of the music, between 0 and 1.
    [[nodiscard]] float musicVolume() const;
    /// Sets the volume of the sound effects, between 0 and 1. Clamped to this range. Applies to the sounds
    /// played from now on.
    void setEffectsVolume(float volume);
    /// Volume of the sound effects, between 0 and 1.
    [[nodiscard]] float effectsVolume() const;
};

} // namespace rtype::audio
