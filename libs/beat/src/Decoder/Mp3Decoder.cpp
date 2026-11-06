#include "rtype/beat/Decoder/Mp3Decoder.hpp"

// raylib embeds its own dr_mp3 in raudio.c, and the client links both: keep the functions of this copy private to
// this file, or the linker finds every drmp3_* defined twice
#define DRMP3_API static
#define DR_MP3_IMPLEMENTATION
#include <dr_mp3.h>

#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace rtype::beat
{

namespace
{

std::vector<char> readFile(const std::filesystem::path &file)
{
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        throw std::runtime_error(std::format("{}: cannot be read", file.generic_string()));
    }
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

MonoSound mixToMono(const float *frames, std::uint64_t frameCount, std::uint32_t channels,
                    std::uint32_t sampleRate)
{
    MonoSound sound;
    sound.sampleRate = sampleRate;
    sound.samples.resize(frameCount);
    for (std::uint64_t frame = 0; frame < frameCount; ++frame) {
        float sum = 0.0f;
        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            sum += frames[frame * channels + channel];
        }
        sound.samples[frame] = sum / static_cast<float>(channels);
    }
    return sound;
}

} // namespace

MonoSound decodeMp3(const std::filesystem::path &file)
{
    const std::vector<char> content = readFile(file);
    drmp3_config config{};
    drmp3_uint64 frameCount = 0;
    float *frames =
        drmp3_open_memory_and_read_pcm_frames_f32(content.data(), content.size(), &config, &frameCount, nullptr);
    if (frames == nullptr || config.channels == 0) {
        throw std::runtime_error(std::format("{}: not a valid mp3", file.generic_string()));
    }
    MonoSound sound = mixToMono(frames, frameCount, config.channels, config.sampleRate);
    drmp3_free(frames, nullptr);
    return sound;
}

} // namespace rtype::beat
