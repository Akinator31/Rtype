#pragma once

#include <array>

/// VR through OpenXR (docs/architecture/vr.md, ADR 0037): the session of a headset, its eyes and its
/// controllers. No OpenXR type appears in the public headers.
namespace rtype::xr
{

/// A point or a direction, in metres. Right-handed, as OpenXR: x to the right, y up, -z ahead.
struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    bool operator==(const Vector3 &) const = default;
};

/// A rotation, as a unit quaternion.
struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    bool operator==(const Quaternion &) const = default;
};

/// Where something is and where it faces, in the space of the headset.
struct Pose {
    Vector3 position;
    Quaternion orientation;
};

/// A 4x4 matrix stored column by column, as OpenGL and raylib's rlgl read it: `values[12]`, `values[13]` and
/// `values[14]` are the translation.
struct Matrix {
    std::array<float, 16> values{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

    /// The value at a row and a column, from 0.
    [[nodiscard]] float at(int row, int column) const { return values[(column * 4) + row]; }
};

/// What an eye sees: the angles of the four sides of its view from its axis, in radians. Left and down are
/// negative. A headset gives each eye an asymmetric field of view, wider towards the outside.
struct FieldOfView {
    float left = 0.0f;
    float right = 0.0f;
    float up = 0.0f;
    float down = 0.0f;
};

/// The product a * b: b applied first, then a.
[[nodiscard]] Matrix multiply(const Matrix &a, const Matrix &b);

/// The matrix that moves a point from the space of the pose to the space the pose is given in.
[[nodiscard]] Matrix toMatrix(const Pose &pose);

/// The inverse of a matrix made of a rotation and a translation only, as toMatrix() makes: the view matrix of
/// an eye is the inverse of its pose.
[[nodiscard]] Matrix rigidInverse(const Matrix &matrix);

/// The point moved by the matrix.
[[nodiscard]] Vector3 transformPoint(const Matrix &matrix, Vector3 point);

/// The OpenGL projection of a field of view: depth from -1 at `nearest` to 1 at `farthest`, both in metres.
[[nodiscard]] Matrix projection(const FieldOfView &fieldOfView, float nearest, float farthest);

} // namespace rtype::xr
