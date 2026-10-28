#include "rtype/audio/AudioPlayer.hpp"

#include "rtype/audio/AudioDevice.hpp"
#include "rtype/core/AssetFile.hpp"
#include "rtype/core/AsyncResourceRequests.hpp"
#include "rtype/core/ReadyFuture.hpp"
#include "rtype/core/ResourceCache.hpp"

#include <raylib.h>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rtype::audio
{

namespace
{

class PreparedSound
{
    Wave _wave{};

public:
    explicit PreparedSound(const std::filesystem::path &file)
    {
        const std::vector<unsigned char> bytes = core::readAssetFile(file);
        _wave = LoadWaveFromMemory(file.extension().string().c_str(), bytes.data(),
                                   static_cast<int>(bytes.size()));
        if (!IsWaveValid(_wave)) {
            if (_wave.data != nullptr) {
                UnloadWave(_wave);
            }
            throw std::runtime_error("Cannot load sound " + file.string() + ": unsupported format");
        }
    }

    ~PreparedSound() { UnloadWave(_wave); }
    PreparedSound(const PreparedSound &) = delete;
    PreparedSound &operator=(const PreparedSound &) = delete;
    PreparedSound(PreparedSound &&other) noexcept : _wave(std::exchange(other._wave, {})) {}
    PreparedSound &operator=(PreparedSound &&other) noexcept
    {
        if (this != &other) {
            UnloadWave(_wave);
            _wave = std::exchange(other._wave, {});
        }
        return *this;
    }

    [[nodiscard]] Wave wave() const { return _wave; }
};

// Number of copies of a sound that can play at the same time
constexpr std::size_t voicesPerSound = 4;

void requireFile(const std::filesystem::path &file, const char *kind)
{
    // Checked even when the audio is not available, so that a missing file is noticed on every computer
    if (!std::filesystem::is_regular_file(file)) {
        throw std::runtime_error(std::string("Cannot load ") + kind + " " + file.string() +
                                 ": file not found");
    }
}

// Owns a sound effect and its voices. A default-constructed effect plays nothing: it stands for a sound
// loaded while the audio is not available. A moved-from effect frees nothing.
class SoundEffect
{
private:
    // The first voice is the sound itself, which owns the samples; the others are aliases sharing them
    std::vector<::Sound> _voices;
    std::size_t _next = 0;

public:
    SoundEffect() = default;
    explicit SoundEffect(const std::filesystem::path &file)
    {
        const ::Sound source = LoadSound(file.string().c_str());
        if (!IsSoundValid(source)) {
            throw std::runtime_error("Cannot load sound " + file.string() + ": unsupported format");
        }
        _voices.push_back(source);
        for (std::size_t voice = 1; voice < voicesPerSound; ++voice) {
            _voices.push_back(LoadSoundAlias(source));
        }
    }
    explicit SoundEffect(Wave wave)
    {
        const ::Sound source = LoadSoundFromWave(wave);
        if (!IsSoundValid(source)) {
            throw std::runtime_error("Cannot create sound");
        }
        _voices.push_back(source);
        for (std::size_t voice = 1; voice < voicesPerSound; ++voice) {
            _voices.push_back(LoadSoundAlias(source));
        }
    }
    ~SoundEffect()
    {
        if (_voices.empty()) {
            return;
        }
        for (std::size_t voice = 1; voice < _voices.size(); ++voice) {
            UnloadSoundAlias(_voices[voice]);
        }
        UnloadSound(_voices.front());
    }

    SoundEffect(const SoundEffect &) = delete;
    SoundEffect &operator=(const SoundEffect &) = delete;
    SoundEffect(SoundEffect &&other) noexcept
        : _voices(std::exchange(other._voices, {})), _next(std::exchange(other._next, 0))
    {
    }
    SoundEffect &operator=(SoundEffect &&) = delete;

    void play(const PlaySettings &settings, float effectsVolume)
    {
        if (_voices.empty()) {
            return;
        }
        // A free voice if there is one, otherwise the oldest one is restarted
        std::size_t chosen = _next;
        for (std::size_t offset = 0; offset < _voices.size(); ++offset) {
            const std::size_t voice = (_next + offset) % _voices.size();
            if (!IsSoundPlaying(_voices[voice])) {
                chosen = voice;
                break;
            }
        }
        _next = (chosen + 1) % _voices.size();

        ::Sound &sound = _voices[chosen];
        SetSoundVolume(sound, std::clamp(settings.volume, 0.0f, 1.0f) * effectsVolume);
        SetSoundPitch(sound, std::max(settings.pitch, 0.01f));
        SetSoundPan(sound, std::clamp(settings.pan, -1.0f, 1.0f));
        PlaySound(sound);
    }
};

// Owns a music stream. A default-constructed music plays nothing, like a default SoundEffect.
class MusicStream
{
private:
    std::vector<unsigned char> _bytes;
    std::optional<::Music> _music;

public:
    MusicStream() = default;
    explicit MusicStream(const std::filesystem::path &file) : _music(LoadMusicStream(file.string().c_str()))
    {
        if (!IsMusicValid(*_music)) {
            throw std::runtime_error("Cannot load music " + file.string() + ": unsupported format");
        }
    }
    MusicStream(const std::filesystem::path &file, std::vector<unsigned char> bytes)
        : _bytes(std::move(bytes)),
          _music(LoadMusicStreamFromMemory(file.extension().string().c_str(), _bytes.data(),
                                           static_cast<int>(_bytes.size())))
    {
        if (!IsMusicValid(*_music)) {
            throw std::runtime_error("Cannot load music " + file.string() + ": unsupported format");
        }
    }
    ~MusicStream()
    {
        if (_music) {
            UnloadMusicStream(*_music);
        }
    }

    MusicStream(const MusicStream &) = delete;
    MusicStream &operator=(const MusicStream &) = delete;
    MusicStream(MusicStream &&other) noexcept
        : _bytes(std::move(other._bytes)), _music(std::exchange(other._music, std::nullopt))
    {
    }
    MusicStream &operator=(MusicStream &&) = delete;

    // Nothing when the audio is not available
    [[nodiscard]] ::Music *get() { return _music ? &*_music : nullptr; }
};

} // namespace

struct AudioPlayer::Impl {
    bool available;
    std::filesystem::path assetsDirectory;
    std::shared_ptr<core::BackgroundLoader> loader;
    core::ResourceCache<SoundEffect, SoundId> sounds;
    core::ResourceCache<MusicStream, MusicId> musics;
    core::AsyncResourceRequests<SoundId> pendingSounds;
    core::AsyncResourceRequests<MusicId> pendingMusics;
    std::optional<MusicId> currentMusic;
    bool paused = false;
    float musicVolume = 1.0f;
    float effectsVolume = 1.0f;

    Impl(bool audioAvailable, std::filesystem::path assets, std::shared_ptr<core::BackgroundLoader> worker)
        : available(audioAvailable), assetsDirectory(std::move(assets)), loader(std::move(worker)),
          pendingSounds(loader), pendingMusics(loader)
    {
    }

    ::Music *current() { return currentMusic ? musics.get(*currentMusic).get() : nullptr; }
};

AudioPlayer::AudioPlayer(const AudioDevice &device, std::filesystem::path assetsDirectory,
                         std::shared_ptr<core::BackgroundLoader> loader)
    : _impl(std::make_unique<Impl>(device.isAvailable(), std::move(assetsDirectory), std::move(loader)))
{
}

AudioPlayer::~AudioPlayer() = default;
AudioPlayer::AudioPlayer(AudioPlayer &&) noexcept = default;
AudioPlayer &AudioPlayer::operator=(AudioPlayer &&) noexcept = default;

SoundId AudioPlayer::loadSound(const std::string &path)
{
    return _impl->sounds.load(path, [&] {
        const std::filesystem::path file = _impl->assetsDirectory / path;
        requireFile(file, "sound");
        return _impl->available ? SoundEffect(file) : SoundEffect();
    });
}

MusicId AudioPlayer::loadMusic(const std::string &path)
{
    return _impl->musics.load(path, [&] {
        const std::filesystem::path file = _impl->assetsDirectory / path;
        requireFile(file, "music");
        return _impl->available ? MusicStream(file) : MusicStream();
    });
}

std::shared_future<SoundId> AudioPlayer::loadSoundAsync(const std::string &path)
{
    if (const auto loaded = _impl->sounds.findId(path)) {
        return core::makeReadyFuture(*loaded);
    }
    const std::filesystem::path file = _impl->assetsDirectory / path;
    Impl *impl = _impl.get();
    return impl->pendingSounds.request(
        path,
        [file, available = impl->available] {
            if (!available) {
                core::readAssetFile(file);
                return std::optional<PreparedSound>{};
            }
            return std::optional<PreparedSound>(std::in_place, file);
        },
        [impl, path](std::optional<PreparedSound> prepared) {
            return impl->sounds.load(
                path, [&] { return prepared ? SoundEffect(prepared->wave()) : SoundEffect(); });
        });
}

std::shared_future<MusicId> AudioPlayer::loadMusicAsync(const std::string &path)
{
    if (const auto loaded = _impl->musics.findId(path)) {
        return core::makeReadyFuture(*loaded);
    }
    const std::filesystem::path file = _impl->assetsDirectory / path;
    Impl *impl = _impl.get();
    return impl->pendingMusics.request(
        path, [file] { return core::readAssetFile(file); },
        [impl, path, file](std::vector<unsigned char> bytes) {
            return impl->musics.load(
                path, [&] { return impl->available ? MusicStream(file, std::move(bytes)) : MusicStream(); });
        });
}

void AudioPlayer::finishPendingLoads()
{
    _impl->pendingSounds.pump();
    _impl->pendingMusics.pump();
}

bool AudioPlayer::unloadSound(const std::string &path)
{
    _impl->pendingSounds.forgetRequest(path);
    return _impl->sounds.unload(path);
}

bool AudioPlayer::unloadMusic(const std::string &path)
{
    _impl->pendingMusics.forgetRequest(path);
    if (const auto music = _impl->musics.findId(path); music && music == _impl->currentMusic) {
        stopMusic();
    }
    return _impl->musics.unload(path);
}

void AudioPlayer::play(SoundId sound, const PlaySettings &settings)
{
    _impl->sounds.get(sound).play(settings, _impl->effectsVolume);
}

void AudioPlayer::playMusic(MusicId music, bool loop)
{
    stopMusic();
    _impl->currentMusic = music;
    _impl->paused = false;
    if (::Music *stream = _impl->current()) {
        stream->looping = loop;
        SetMusicVolume(*stream, _impl->musicVolume);
        PlayMusicStream(*stream);
    }
}

void AudioPlayer::stopMusic()
{
    if (::Music *stream = _impl->current()) {
        StopMusicStream(*stream);
    }
    _impl->currentMusic.reset();
    _impl->paused = false;
}

void AudioPlayer::pauseMusic()
{
    if (::Music *stream = _impl->current()) {
        PauseMusicStream(*stream);
    }
    _impl->paused = _impl->currentMusic.has_value();
}

void AudioPlayer::resumeMusic()
{
    if (::Music *stream = _impl->current()) {
        ResumeMusicStream(*stream);
    }
    _impl->paused = false;
}

bool AudioPlayer::isMusicPlaying() const
{
    if (!_impl->currentMusic || _impl->paused) {
        return false;
    }
    // Without audio, a music "plays" until it is stopped: there is no stream to tell when it ends
    const ::Music *stream = _impl->current();
    return stream == nullptr || IsMusicStreamPlaying(*stream);
}

void AudioPlayer::update()
{
    if (::Music *stream = _impl->current(); stream != nullptr && !_impl->paused) {
        UpdateMusicStream(*stream);
    }
}

void AudioPlayer::setMusicVolume(float volume)
{
    _impl->musicVolume = std::clamp(volume, 0.0f, 1.0f);
    if (::Music *stream = _impl->current()) {
        SetMusicVolume(*stream, _impl->musicVolume);
    }
}

float AudioPlayer::musicVolume() const
{
    return _impl->musicVolume;
}

void AudioPlayer::setEffectsVolume(float volume)
{
    _impl->effectsVolume = std::clamp(volume, 0.0f, 1.0f);
}

float AudioPlayer::effectsVolume() const
{
    return _impl->effectsVolume;
}

} // namespace rtype::audio
