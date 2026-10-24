#pragma once

#include <cstdint>

namespace rtype::render
{

/// A 2D position or size, in game units (pixels of the game's logical resolution).
struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;
};

/// A 3D position, size or direction, in world units. The y axis points up.
struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

/// How a camera projects the 3D models on the screen.
enum class Projection {
    /// What is far looks smaller, as with an eye.
    Perspective,
    /// Every model keeps its size whatever its distance, as on a plan: the world seen by a game in 2D.
    Orthographic,
};

/// A camera that 3D models are seen through.
struct Camera {
    /// Where the camera is.
    Vector3 position;
    /// The point it looks at. Looking towards -z, x points right on screen.
    Vector3 target{0.0f, 0.0f, -1.0f};
    /// Which direction is up on screen.
    Vector3 up{0.0f, 1.0f, 0.0f};
    /// Vertical field of view, in degrees; for an orthographic camera, the height it sees, in world units.
    float fieldOfView = 45.0f;
    Projection projection = Projection::Perspective;
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

/// A colour at a place of a gradient.
struct ColorStop {
    /// Where the colour is reached, from 0 at the start of the gradient to 1 at its end.
    float position = 0.0f;
    Color color;
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

/// The light of the 3D models: a directional light, such as a sun, plus an ambient light so that the side in
/// the shadow is not completely black.
struct Light {
    /// Direction the light travels in, from the light towards the models. Its length does not matter.
    Vector3 direction{-1.0f, -1.0f, -1.0f};
    /// Colour and strength of the directional light.
    Color color = colors::White;
    /// Light received everywhere, including in the shadow.
    Color ambient{20, 20, 26, 255};
};

} // namespace rtype::render
