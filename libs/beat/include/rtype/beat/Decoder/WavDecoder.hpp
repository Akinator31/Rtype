#pragma once

#include "rtype/beat/Decoder/Mp3Decoder.hpp"

namespace rtype::beat
{

/// Decodes a wav file, its channels mixed into one. Throws std::runtime_error if the file cannot be read or
/// is not a valid wav.
[[nodiscard]] MonoSound decodeWav(const std::filesystem::path &file);

/// Decodes an mp3 or a wav file, chosen by the extension of the path. Throws std::runtime_error if the file
/// cannot be read, is not valid, or has another extension.
[[nodiscard]] MonoSound decodeSound(const std::filesystem::path &file);

} // namespace rtype::beat
