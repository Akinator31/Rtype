#include "rtype/beat/Decoder/WavDecoder.hpp"

// raylib embeds its own dr_wav in raudio.c, and the client links both: keep the functions of this copy
// private to this file, or the linker finds every drwav_* defined twice
#define DRWAV_API static
#define DRWAV_PRIVATE static
#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#include <algorithm>
#include <cctype>
#include <format>
#include <stdexcept>
#include <string>

namespace rtype::beat
{

namespace
{

MonoSound mixToMono(const std::vector<float> &frames, std::uint32_t channels, std::uint32_t sampleRate)
{
    MonoSound sound;
    sound.sampleRate = sampleRate;
    sound.samples.resize(frames.size() / channels);
    for (std::size_t frame = 0; frame < sound.samples.size(); ++frame) {
        float sum = 0.0f;
        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            sum += frames[frame * channels + channel];
        }
        sound.samples[frame] = sum / static_cast<float>(channels);
    }
    return sound;
}

std::string lowerExtension(const std::filesystem::path &file)
{
    std::string extension = file.extension().string();
    std::ranges::transform(extension, extension.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension;
}

} // namespace

MonoSound decodeWav(const std::filesystem::path &file)
{
    drwav wav;
    if (!drwav_init_file(&wav, file.string().c_str(), nullptr) || wav.channels == 0) {
        throw std::runtime_error(std::format("{}: not a valid wav", file.generic_string()));
    }
    std::vector<float> frames(static_cast<std::size_t>(wav.totalPCMFrameCount) * wav.channels);
    const drwav_uint64 read = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, frames.data());
    frames.resize(static_cast<std::size_t>(read) * wav.channels);
    MonoSound sound = mixToMono(frames, wav.channels, wav.sampleRate);
    drwav_uninit(&wav);
    return sound;
}

MonoSound decodeSound(const std::filesystem::path &file)
{
    const std::string extension = lowerExtension(file);
    if (extension == ".wav") {
        return decodeWav(file);
    }
    if (extension == ".mp3") {
        return decodeMp3(file);
    }
    throw std::runtime_error(std::format("{}: only mp3 and wav are read", file.generic_string()));
}

} // namespace rtype::beat
