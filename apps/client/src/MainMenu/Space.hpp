#pragma once

#include "rtype/render/Types.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

/// Small vector maths for placing and flying models in the 3D world of the menu.
namespace rtype::client
{

/// What the rotation of a Model needs: an axis and an angle, in degrees.
struct Rotation {
    render::Vector3 axis{0.0f, 1.0f, 0.0f};
    float angle = 0.0f;
};

[[nodiscard]] inline render::Vector3 operator+(render::Vector3 left, render::Vector3 right)
{
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] inline render::Vector3 operator-(render::Vector3 left, render::Vector3 right)
{
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] inline render::Vector3 operator*(render::Vector3 vector, float factor)
{
    return {vector.x * factor, vector.y * factor, vector.z * factor};
}

[[nodiscard]] inline float dot(render::Vector3 left, render::Vector3 right)
{
    return (left.x * right.x) + (left.y * right.y) + (left.z * right.z);
}

[[nodiscard]] inline render::Vector3 cross(render::Vector3 left, render::Vector3 right)
{
    return {(left.y * right.z) - (left.z * right.y), (left.z * right.x) - (left.x * right.z),
            (left.x * right.y) - (left.y * right.x)};
}

[[nodiscard]] inline float length(render::Vector3 vector)
{
    return std::sqrt(dot(vector, vector));
}

/// The vector scaled to a length of 1; a null vector stays null.
[[nodiscard]] inline render::Vector3 normalize(render::Vector3 vector)
{
    const float size = length(vector);
    return size > 0.0f ? vector * (1.0f / size) : vector;
}

[[nodiscard]] inline float toRadians(float degrees)
{
    return degrees * std::numbers::pi_v<float> / 180.0f;
}

[[nodiscard]] inline float toDegrees(float radians)
{
    return radians * 180.0f / std::numbers::pi_v<float>;
}

/// Turns a vector around an axis by an angle in degrees, counterclockwise when the axis points at the viewer,
/// like a Model is turned (Rodrigues' formula).
[[nodiscard]] inline render::Vector3 rotate(render::Vector3 vector, Rotation rotation)
{
    const render::Vector3 axis = normalize(rotation.axis);
    const float angle = toRadians(rotation.angle);
    const float cosine = std::cos(angle);
    return (vector * cosine) + (cross(axis, vector) * std::sin(angle)) +
           (axis * (dot(axis, vector) * (1.0f - cosine)));
}

/// The rotation that turns a model whose nose points to +z and top to +y so that its nose points to
/// `forward` and its top as close as possible to `up`. Both must be unit vectors, not parallel.
[[nodiscard]] inline Rotation computeRotationTowards(render::Vector3 forward, render::Vector3 up)
{
    const render::Vector3 side = normalize(cross(up, forward));
    const render::Vector3 top = cross(forward, side);
    // Columns of the rotation matrix: where the x, y and z axes of the model go
    const float trace = side.x + top.y + forward.z;
    const float cosine = std::clamp((trace - 1.0f) / 2.0f, -1.0f, 1.0f);
    const float angle = std::acos(cosine);
    const render::Vector3 axis{top.z - forward.y, forward.x - side.z, side.y - top.x};
    if (length(axis) > 1e-4f) {
        return {normalize(axis), toDegrees(angle)};
    }
    if (cosine > 0.0f) {
        return {};
    }
    // Half a turn: the matrix is 2 * axis * axis^T - identity, its diagonal gives the size of each component
    // and the products of components give their signs, taken from the first component that is not null
    const float x = std::sqrt(std::max(0.0f, (side.x + 1.0f) / 2.0f));
    const float y = std::sqrt(std::max(0.0f, (top.y + 1.0f) / 2.0f));
    const float z = std::sqrt(std::max(0.0f, (forward.z + 1.0f) / 2.0f));
    if (x > 1e-4f) {
        return {normalize({x, std::copysign(y, side.y), std::copysign(z, side.z)}), 180.0f};
    }
    if (y > 1e-4f) {
        return {normalize({0.0f, y, std::copysign(z, top.z)}), 180.0f};
    }
    return {{0.0f, 0.0f, 1.0f}, 180.0f};
}

} // namespace rtype::client
