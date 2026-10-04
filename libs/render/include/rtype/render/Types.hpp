#pragma once

#include <cstdint>

namespace rtype::render
{

/// A 2D position or size, in game units (pixels of the game's logical resolution).
struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;
};

/// An axis-aligned rectangle: its top-left corner and its size.
struct Rectangle {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

/// A colour, with an alpha channel for transparency (0 is invisible, 255 is opaque).
struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

/// Common colours.
namespace colors
{
inline constexpr Color Black{0, 0, 0, 255};
inline constexpr Color White{255, 255, 255, 255};
inline constexpr Color Red{230, 41, 55, 255};
inline constexpr Color Green{0, 228, 48, 255};
inline constexpr Color Blue{0, 121, 241, 255};
inline constexpr Color Yellow{253, 249, 0, 255};
inline constexpr Color Gray{130, 130, 130, 255};
inline constexpr Color Transparent{0, 0, 0, 0};
} // namespace colors

} // namespace rtype::render
