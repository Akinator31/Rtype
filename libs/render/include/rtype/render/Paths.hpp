#pragma once

#include <filesystem>

namespace rtype::render
{

/// Directory containing the running executable.
///
/// Files shipped with the game (assets) are looked up from here rather than from the current directory,
/// so that the game finds them wherever it is started from.
[[nodiscard]] std::filesystem::path applicationDirectory();

/// Directory where the game keeps the files of the player (its save), which may not exist yet:
/// `%APPDATA%\R-Type` on Windows, `$XDG_DATA_HOME/r-type` or `~/.local/share/r-type` elsewhere. The
/// application directory if none of these is known.
///
/// Not next to the executable, which the player may not be allowed to write once the game is installed.
[[nodiscard]] std::filesystem::path userDataDirectory();

} // namespace rtype::render
