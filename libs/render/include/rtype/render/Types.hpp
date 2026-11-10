#pragma once

#include <array>
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

/// A 4x4 matrix stored column by column, as OpenGL reads it: `values[12]`, `values[13]` and `values[14]` are
/// the translation. The identity by default.
struct Matrix {
    std::array<float, 16> values{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

    /// The value at a row and a column, from 0.
    [[nodiscard]] float at(int row, int column) const { return values[(column * 4) + row]; }
};

/// What an eye of a VR headset sees, for Renderer::beginEye(). The space it is given in is the world of the
/// eye pass: in metres, y up, -z ahead.
struct EyeView {
    /// From the world to the space of the eye.
    Matrix view;
    /// OpenGL projection of the eye, depth in [-1, 1].
    Matrix projection;
    /// Where the eye is, in the world.
    Vector3 position;
    /// Size of the image of the eye, in pixels.
    int width = 0;
    int height = 0;
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
/// the shadow is not completely black, and optionally a rim light from behind, which outlines the edges of
/// the models to cut them out of the background.
struct Light {
    /// Direction the light travels in, from the light towards the models. Its length does not matter.
    Vector3 direction{-1.0f, -1.0f, -1.0f};
    /// Colour and strength of the directional light.
    Color color = colors::White;
    /// Light received everywhere, including in the shadow. It comes more from the top of the screen than from
    /// below.
    Color ambient{20, 20, 26, 255};
    /// Direction the rim light travels in: from behind the models, towards the camera. Its length does not
    /// matter.
    Vector3 rimDirection{0.4f, -0.3f, 1.0f};
    /// Colour and strength of the rim light, black (off) by default.
    Color rim = colors::Black;
};

/// A pose of an animated model, from its animations (indexes from ResourceManager::getModelAnimation()), at
/// times in seconds; the animations loop.
struct ModelPose {
    /// Between a time of `from`, at an amount of 0, and a time of `to`, at an amount of 1. A single
    /// animation is `to`, at an amount of 1.
    int from = -1;
    float fromTime = 0.0f;
    int to = -1;
    float toTime = 0.0f;
    float amount = 1.0f;
    /// Added over them, -1 for none: what this animation changes from its first frame adds to the bones it
    /// moves, each in the space of its parent, such as the recoil of a gun and the flash at its muzzle over
    /// a flight. The other bones keep their pose.
    int added = -1;
    float addedTime = 0.0f;

    friend bool operator==(const ModelPose &, const ModelPose &) = default;
};

} // namespace rtype::render
