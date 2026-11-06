#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace rtype::beat
{

/// A decoded sound: one channel, samples from -1 to 1.
struct MonoSound {
    std::vector<float> samples;
    std::uint32_t sampleRate = 0;

    [[nodiscard]] float duration() const
    {
        return sampleRate == 0 ? 0.0f : static_cast<float>(samples.size()) / static_cast<float>(sampleRate);
    }
};

/// Decodes an mp3 file, its channels mixed into one. Throws std::runtime_error if the file cannot be read or
/// is not a valid mp3.
[[nodiscard]] MonoSound decodeMp3(const std::filesystem::path &file);

} // namespace rtype::beat
