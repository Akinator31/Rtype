#pragma once

#include <vector>

namespace rtype::render
{

/// The characters of `wanted` that a font file can draw, read from its table of characters (cmap). A font is
/// loaded with these only: asking for a character it lacks would draw nothing more, and makes raylib warn.
/// @param file The content of a TrueType or OpenType font file.
/// @param wanted Unicode code points.
/// @return The code points of `wanted` the font has, in the same order. All of `wanted` if the table cannot
/// be read: raylib then loads the font as before.
[[nodiscard]] std::vector<int> filterCharactersInFont(const std::vector<unsigned char> &file,
                                                      const std::vector<int> &wanted);

} // namespace rtype::render
