#include "Vr/VrPointer.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace rtype::client
{

namespace
{

/// Below this, a coefficient is taken as zero: the panel flat, the ray along it.
constexpr float Tiny = 1e-6f;
/// A hit nearer than this to the start of the ray is the controller itself, not the panel.
constexpr float Nearest = 1e-4f;
/// The box of a controller, in metres: across, its thickness, along the grip.
constexpr render::Vector3 ControllerSize{0.04f, 0.035f, 0.12f};
/// How far its middle is ahead of the palm, along the grip.
constexpr float ControllerAhead = 0.02f;

/// The rotation and the scale of an affine transform inverted, by its cofactors: the transform of the panel
/// is scaled differently along its width and its height, so it is not a rotation.
std::array<float, 9> invertLinear(const render::Matrix &matrix)
{
    const auto m = [&](int row, int column) { return matrix.at(row, column); };
    const float c00 = (m(1, 1) * m(2, 2)) - (m(1, 2) * m(2, 1));
    const float c01 = (m(1, 2) * m(2, 0)) - (m(1, 0) * m(2, 2));
    const float c02 = (m(1, 0) * m(2, 1)) - (m(1, 1) * m(2, 0));
    const float determinant = (m(0, 0) * c00) + (m(0, 1) * c01) + (m(0, 2) * c02);
    if (std::abs(determinant) < Tiny) {
        return {};
    }
    const float inverse = 1.0f / determinant;
    // Row by row
    return {c00 * inverse,
            ((m(0, 2) * m(2, 1)) - (m(0, 1) * m(2, 2))) * inverse,
            ((m(0, 1) * m(1, 2)) - (m(0, 2) * m(1, 1))) * inverse,
            c01 * inverse,
            ((m(0, 0) * m(2, 2)) - (m(0, 2) * m(2, 0))) * inverse,
            ((m(0, 2) * m(1, 0)) - (m(0, 0) * m(1, 2))) * inverse,
            c02 * inverse,
            ((m(0, 1) * m(2, 0)) - (m(0, 0) * m(2, 1))) * inverse,
            ((m(0, 0) * m(1, 1)) - (m(0, 1) * m(1, 0))) * inverse};
}

render::Vector3 apply(const std::array<float, 9> &rows, render::Vector3 vector)
{
    return {(rows[0] * vector.x) + (rows[1] * vector.y) + (rows[2] * vector.z),
            (rows[3] * vector.x) + (rows[4] * vector.y) + (rows[5] * vector.z),
            (rows[6] * vector.x) + (rows[7] * vector.y) + (rows[8] * vector.z)};
}

render::Vector3 transformPoint(const render::Matrix &matrix, render::Vector3 point)
{
    const auto &m = matrix.values;
    return {(m[0] * point.x) + (m[4] * point.y) + (m[8] * point.z) + m[12],
            (m[1] * point.x) + (m[5] * point.y) + (m[9] * point.z) + m[13],
            (m[2] * point.x) + (m[6] * point.y) + (m[10] * point.z) + m[14]};
}

render::Matrix toRender(const xr::Matrix &matrix)
{
    render::Matrix result;
    result.values = matrix.values;
    return result;
}

} // namespace

Ray aimRay(const xr::Pose &aim)
{
    const xr::Matrix matrix = xr::toMatrix(aim);
    // The third column is the +z of the pose: it points along -z
    const render::Vector3 ahead{-matrix.at(0, 2), -matrix.at(1, 2), -matrix.at(2, 2)};
    const float length = std::sqrt((ahead.x * ahead.x) + (ahead.y * ahead.y) + (ahead.z * ahead.z));
    return {.origin = {aim.position.x, aim.position.y, aim.position.z},
            .direction = length > 0.0f ? render::Vector3{ahead.x / length, ahead.y / length, ahead.z / length}
                                       : render::Vector3{0.0f, 0.0f, -1.0f}};
}

std::optional<PanelHit> hitPanel(const Ray &ray, const PanelSurface &panel)
{
    const std::array<float, 9> inverse = invertLinear(panel.transform);
    if (inverse == std::array<float, 9>{}) {
        return std::nullopt;
    }
    // The ray in the space of the square, where the panel is z = 4 bend x^2 (drawImageQuad() bends it by its
    // width from -1 to 1, the square goes from -0.5 to 0.5); the same t along both
    const auto &m = panel.transform.values;
    const render::Vector3 origin =
        apply(inverse, {ray.origin.x - m[12], ray.origin.y - m[13], ray.origin.z - m[14]});
    const render::Vector3 direction = apply(inverse, ray.direction);
    const float curve = 4.0f * panel.bend;
    // curve (ox + t dx)^2 - (oz + t dz) = 0
    const float a = curve * direction.x * direction.x;
    const float b = (2.0f * curve * origin.x * direction.x) - direction.z;
    const float c = (curve * origin.x * origin.x) - origin.z;
    std::array<float, 2> roots{-1.0f, -1.0f};
    if (std::abs(a) < Tiny) {
        if (std::abs(b) < Tiny) {
            return std::nullopt;
        }
        roots[0] = -c / b;
    } else {
        const float discriminant = (b * b) - (4.0f * a * c);
        if (discriminant < 0.0f) {
            return std::nullopt;
        }
        const float root = std::sqrt(discriminant);
        roots = {(-b - root) / (2.0f * a), (-b + root) / (2.0f * a)};
        std::ranges::sort(roots);
    }
    for (const float t : roots) {
        if (t < Nearest) {
            continue;
        }
        const float x = origin.x + (t * direction.x);
        const float y = origin.y + (t * direction.y);
        if (std::abs(x) > 0.5f || std::abs(y) > 0.5f) {
            continue;
        }
        const render::Vector3 point{ray.origin.x + (t * ray.direction.x),
                                    ray.origin.y + (t * ray.direction.y),
                                    ray.origin.z + (t * ray.direction.z)};
        // The top of the square (+y) is the top of the frame, y = 0 in game units
        return PanelHit{.point = point,
                        .game = {(x + 0.5f) * panel.gameSize.x, (0.5f - y) * panel.gameSize.y},
                        .distance = t};
    }
    return std::nullopt;
}

render::Matrix controllerTransform(const xr::Pose &grip)
{
    render::Matrix box = toRender(xr::toMatrix(grip));
    const render::Vector3 middle = transformPoint(box, {0.0f, 0.0f, -ControllerAhead});
    for (int row = 0; row < 3; ++row) {
        box.values[static_cast<std::size_t>(row)] *= ControllerSize.x;
        box.values[static_cast<std::size_t>(4 + row)] *= ControllerSize.y;
        box.values[static_cast<std::size_t>(8 + row)] *= ControllerSize.z;
    }
    box.values[12] = middle.x;
    box.values[13] = middle.y;
    box.values[14] = middle.z;
    return box;
}

} // namespace rtype::client
