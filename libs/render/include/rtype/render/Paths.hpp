#pragma once

#include <filesystem>

namespace rtype::render
{

/// Directory containing the running executable.
///
/// Files shipped with the game (assets) are looked up from here rather than from the current directory,
/// so that the game finds them wherever it is started from.
[[nodiscard]] std::filesystem::path applicationDirectory();

} // namespace rtype::render
