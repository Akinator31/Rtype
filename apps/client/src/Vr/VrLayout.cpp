#include "Vr/VrLayout.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace rtype::client
{

namespace
{

render::Vector3 transformPoint(const render::Matrix &matrix, render::Vector3 point)
{
    const auto &m = matrix.values;
    return {(m[0] * point.x) + (m[4] * point.y) + (m[8] * point.z) + m[12],
            (m[1] * point.x) + (m[5] * point.y) + (m[9] * point.z) + m[13],
            (m[2] * point.x) + (m[6] * point.y) + (m[10] * point.z) + m[14]};
}

float squaredDistance(render::Vector3 a, render::Vector3 b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return (x * x) + (y * y) + (z * z);
}

} // namespace

render::Matrix boardTransform(const VrLayout &layout, render::Vector2 gameSize)
{
    const float height = gameSize.x > 0.0f ? layout.boardWidth * gameSize.y / gameSize.x : 0.0f;
    // Turned around x so that the board faces the eyes, over which its centre is: its normal (+z) points
    // back to them
    const float tilt = std::atan2(layout.boardRise, layout.boardDistance);
    const float cosine = std::cos(tilt);
    const float sine = std::sin(tilt);
    render::Matrix transform;
    // Column by column: the width along x, the height along the tilted y, the tilted normal, the centre
    transform.values = {layout.boardWidth,
                        0.0f,
                        0.0f,
                        0.0f,
                        0.0f,
                        height * cosine,
                        height * sine,
                        0.0f,
                        0.0f,
                        -sine,
                        cosine,
                        0.0f,
                        0.0f,
                        layout.boardRise,
                        -layout.boardDistance,
                        1.0f};
    return transform;
}

std::array<render::Vector3, 4> boardCorners(const VrLayout &layout, render::Vector2 gameSize)
{
    const render::Matrix transform = boardTransform(layout, gameSize);
    return {transformPoint(transform, {-0.5f, 0.5f, 0.0f}), transformPoint(transform, {0.5f, 0.5f, 0.0f}),
            transformPoint(transform, {0.5f, -0.5f, 0.0f}), transformPoint(transform, {-0.5f, -0.5f, 0.0f})};
}

bool isSeen(render::Vector3 point, const xr::FieldOfView &fieldOfView)
{
    const float ahead = -point.z;
    if (ahead <= 0.0f) {
        return false;
    }
    const float across = std::atan2(point.x, ahead);
    const float upward = std::atan2(point.y, ahead);
    return across >= fieldOfView.left && across <= fieldOfView.right && upward >= fieldOfView.down &&
           upward <= fieldOfView.up;
}

float elevation(render::Vector3 point)
{
    const float flat = std::hypot(point.x, point.z);
    return std::atan2(point.y, flat) * 180.0f / std::numbers::pi_v<float>;
}

CockpitPlacement placeCockpit(render::Vector3 eyePoint)
{
    // Turned by a quarter around y, the nose (+x) goes ahead (-z) and the right of the pilot (+z) to the
    // right (+x): (x, y, z) becomes (z, y, -x). Then moved so that the eye point is at the origin.
    return {.position = {-eyePoint.z, -eyePoint.y, eyePoint.x},
            .rotationAxis = {0.0f, 1.0f, 0.0f},
            .rotationAngle = 90.0f};
}

render::Vector3 cockpitToHeadset(render::Vector3 point, render::Vector3 eyePoint)
{
    const CockpitPlacement placement = placeCockpit(eyePoint);
    return {point.z + placement.position.x, point.y + placement.position.y, -point.x + placement.position.z};
}

std::vector<int> farToNear(const std::vector<render::ModelMesh> &meshes,
                           std::span<const std::string_view> names, render::Vector3 eyePoint)
{
    std::vector<int> indexes;
    for (std::size_t index = 0; index < meshes.size(); ++index) {
        if (std::ranges::find(names, meshes[index].name) != names.end()) {
            indexes.push_back(static_cast<int>(index));
        }
    }
    std::ranges::stable_sort(indexes, std::ranges::greater{}, [&](int index) {
        return squaredDistance(meshes[static_cast<std::size_t>(index)].centre, eyePoint);
    });
    return indexes;
}

} // namespace rtype::client
