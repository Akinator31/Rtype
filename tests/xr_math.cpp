#include "rtype/xr/Math.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

using rtype::xr::FieldOfView;
using rtype::xr::Matrix;
using rtype::xr::Pose;
using rtype::xr::Vector3;

namespace
{

constexpr float Tolerance = 1e-5f;

bool close(float actual, float expected)
{
    return std::abs(actual - expected) < Tolerance;
}

bool samePoint(Vector3 actual, Vector3 expected)
{
    return close(actual.x, expected.x) && close(actual.y, expected.y) && close(actual.z, expected.z);
}

bool isIdentity(const Matrix &matrix)
{
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            if (!close(matrix.at(row, column), row == column ? 1.0f : 0.0f)) {
                return false;
            }
        }
    }
    return true;
}

/// A rotation of `angle` radians around the y axis (up), as a quaternion.
rtype::xr::Quaternion aroundUp(float angle)
{
    return {0.0f, std::sin(angle / 2.0f), 0.0f, std::cos(angle / 2.0f)};
}

/// Where a point of the space of the eye lands on the screen, from -1 to 1, and its depth.
Vector3 project(const Matrix &projection, Vector3 point)
{
    const auto &m = projection.values;
    const float x = (m[0] * point.x) + (m[4] * point.y) + (m[8] * point.z) + m[12];
    const float y = (m[1] * point.x) + (m[5] * point.y) + (m[9] * point.z) + m[13];
    const float z = (m[2] * point.x) + (m[6] * point.y) + (m[10] * point.z) + m[14];
    const float w = (m[3] * point.x) + (m[7] * point.y) + (m[11] * point.z) + m[15];
    return {x / w, y / w, z / w};
}

} // namespace

TEST_CASE("A pose without rotation is a translation", "[xr][math]")
{
    const Matrix matrix = rtype::xr::toMatrix({.position = {1.0f, 2.0f, 3.0f}, .orientation = {}});

    REQUIRE(samePoint(rtype::xr::transformPoint(matrix, {0.0f, 0.0f, 0.0f}), {1.0f, 2.0f, 3.0f}));
    REQUIRE(samePoint(rtype::xr::transformPoint(matrix, {1.0f, 0.0f, -1.0f}), {2.0f, 2.0f, 2.0f}));
}

TEST_CASE("A quarter turn to the left makes ahead point left", "[xr][math]")
{
    // OpenXR is right-handed, -z ahead: turning left around y brings -z to -x
    const Matrix matrix =
        rtype::xr::toMatrix({.position = {}, .orientation = aroundUp(std::numbers::pi_v<float> / 2)});

    REQUIRE(samePoint(rtype::xr::transformPoint(matrix, {0.0f, 0.0f, -1.0f}), {-1.0f, 0.0f, 0.0f}));
    REQUIRE(samePoint(rtype::xr::transformPoint(matrix, {0.0f, 1.0f, 0.0f}), {0.0f, 1.0f, 0.0f}));
}

TEST_CASE("The view of an eye is the inverse of its pose", "[xr][math]")
{
    const Pose eye{.position = {0.03f, 1.2f, -0.4f}, .orientation = aroundUp(0.7f)};
    const Matrix pose = rtype::xr::toMatrix(eye);
    const Matrix view = rtype::xr::rigidInverse(pose);

    REQUIRE(isIdentity(rtype::xr::multiply(view, pose)));
    REQUIRE(isIdentity(rtype::xr::multiply(pose, view)));
    // The eye itself is at the origin of its view
    REQUIRE(samePoint(rtype::xr::transformPoint(view, eye.position), {0.0f, 0.0f, 0.0f}));
}

TEST_CASE("Multiplying applies the right matrix first", "[xr][math]")
{
    const Matrix moved = rtype::xr::toMatrix({.position = {1.0f, 0.0f, 0.0f}, .orientation = {}});
    const Matrix turned =
        rtype::xr::toMatrix({.position = {}, .orientation = aroundUp(std::numbers::pi_v<float> / 2)});

    // Turned, then moved: (0, 0, -1) goes to (-1, 0, 0), then to (0, 0, 0)
    REQUIRE(samePoint(rtype::xr::transformPoint(rtype::xr::multiply(moved, turned), {0.0f, 0.0f, -1.0f}),
                      {0.0f, 0.0f, 0.0f}));
}

TEST_CASE("A symmetric field of view projects like a perspective", "[xr][math]")
{
    const float half = std::numbers::pi_v<float> / 4; // 90 degrees wide and high
    const Matrix projection = rtype::xr::projection({-half, half, half, -half}, 0.1f, 100.0f);

    // The centre of the view, the edges at 45 degrees, and the depth of near and far
    REQUIRE(close(project(projection, {0.0f, 0.0f, -2.0f}).x, 0.0f));
    REQUIRE(close(project(projection, {0.0f, 0.0f, -2.0f}).y, 0.0f));
    REQUIRE(close(project(projection, {2.0f, 0.0f, -2.0f}).x, 1.0f));
    REQUIRE(close(project(projection, {0.0f, -2.0f, -2.0f}).y, -1.0f));
    REQUIRE(project(projection, {0.0f, 0.0f, -0.1f}).z == Catch::Approx(-1.0f));
    REQUIRE(project(projection, {0.0f, 0.0f, -100.0f}).z == Catch::Approx(1.0f));
}

TEST_CASE("An asymmetric field of view puts its edges at their angles", "[xr][math]")
{
    // The left eye of a headset: wider towards the outside (left) than towards the nose
    const FieldOfView fieldOfView{.left = -0.9f, .right = 0.7f, .up = 0.8f, .down = -0.85f};
    const Matrix projection = rtype::xr::projection(fieldOfView, 0.05f, 200.0f);
    const float distance = 3.0f;

    REQUIRE(close(project(projection, {distance * std::tan(-0.9f), 0.0f, -distance}).x, -1.0f));
    REQUIRE(close(project(projection, {distance * std::tan(0.7f), 0.0f, -distance}).x, 1.0f));
    REQUIRE(close(project(projection, {0.0f, distance * std::tan(0.8f), -distance}).y, 1.0f));
    REQUIRE(close(project(projection, {0.0f, distance * std::tan(-0.85f), -distance}).y, -1.0f));
    // Straight ahead is not the centre of the image
    REQUIRE(project(projection, {0.0f, 0.0f, -distance}).x > 0.0f);
}
