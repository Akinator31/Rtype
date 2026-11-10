#include "Vr/VrLayout.hpp"

#include "Vr/CockpitLook.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace rtype::client
{

using render::transformPoint;

namespace
{

float squaredDistance(render::Vector3 a, render::Vector3 b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return (x * x) + (y * y) + (z * z);
}

/// A rectangle of this size centred on a point, tilted around x to face the eyes at the origin: its normal
/// (+z) points back to them.
render::Matrix facingTransform(render::Vector3 centre, float width, float height)
{
    const float tilt = std::atan2(centre.y, -centre.z);
    const float cosine = std::cos(tilt);
    const float sine = std::sin(tilt);
    render::Matrix transform;
    // Column by column: the width along x, the height along the tilted y, the tilted normal, the centre
    transform.values = {width, 0.0f,  0.0f,   0.0f, 0.0f,     height * cosine, height * sine, 0.0f,
                        0.0f,  -sine, cosine, 0.0f, centre.x, centre.y,        centre.z,      1.0f};
    return transform;
}

float heightOf(float width, render::Vector2 gameSize)
{
    return gameSize.x > 0.0f ? width * gameSize.y / gameSize.x : 0.0f;
}

float smooth(float value)
{
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return clamped * clamped * (3.0f - (2.0f * clamped));
}

float lerp(float from, float to, float amount)
{
    return from + ((to - from) * amount);
}

/// The two moments of the unfolding: the line rises, then the panel opens.
constexpr float RiseShare = 0.4f;
/// The width of the line of light, and its height, as parts of those of the panel.
constexpr float LineWidth = 0.1f;
constexpr float LineHeight = 0.01f;

} // namespace

render::Matrix boardTransform(const VrLayout &layout, render::Vector2 gameSize)
{
    return facingTransform({0.0f, layout.boardRise, -layout.boardDistance}, layout.boardWidth,
                           heightOf(layout.boardWidth, gameSize));
}

render::BoardView boardView(const VrLayout &layout, render::Vector2 gameSize, float brightness)
{
    return {.transform = boardTransform(layout, gameSize),
            .area = {0.0f, 0.0f, gameSize.x, gameSize.y},
            .lift = layout.reliefLift,
            .overlay = layout.reliefOverlay,
            .brightness = std::clamp(brightness, 0.0f, 1.0f)};
}

bool canDrawInRelief(const std::optional<render::Camera> &camera)
{
    return !camera || camera->projection == render::Projection::Orthographic;
}

engine::SurfaceAreas surfaceAreas(render::Vector2 gameSize)
{
    engine::SurfaceAreas areas{};
    const render::Rectangle game{0.0f, 0.0f, gameSize.x, gameSize.y};
    areas.fill(game);
    for (const CockpitScreen &screen : CockpitScreens) {
        areas[static_cast<std::size_t>(screen.surface)] = {0.0f, 0.0f, screen.size.x, screen.size.y};
    }
    return areas;
}

render::Matrix panelTransform(const VrLayout &layout, render::Vector2 gameSize, float openness)
{
    const float rise = smooth(openness / RiseShare);
    const float spread = smooth((openness - RiseShare) / (1.0f - RiseShare));
    const render::Vector3 open{0.0f, layout.panelRise, -layout.panelDistance};
    const render::Vector3 centre{lerp(layout.panelOrigin.x, open.x, rise),
                                 lerp(layout.panelOrigin.y, open.y, rise),
                                 lerp(layout.panelOrigin.z, open.z, rise)};
    const float width = layout.panelWidth * (LineWidth + ((0.5f - LineWidth) * rise) + (0.5f * spread));
    const float height = heightOf(layout.panelWidth, gameSize) * lerp(LineHeight, 1.0f, spread);
    return facingTransform(centre, width, height);
}

render::ImageQuadLook panelLook(const VrLayout &layout, float openness, float brightness)
{
    const float spread = smooth((openness - RiseShare) / (1.0f - RiseShare));
    const auto channel = static_cast<unsigned char>(std::lround(std::clamp(brightness, 0.0f, 1.0f) * 255.0f));
    return {.tint = {channel, channel, channel, 255}, .bend = layout.panelBend * spread, .onTop = true};
}

float boardBrightness(float openness, float brightness, bool playing)
{
    const float behind = playing ? BoardBehindPanel : 1.0f;
    return std::clamp(brightness, 0.0f, 1.0f) * lerp(1.0f, behind, smooth(openness));
}

std::array<render::Vector3, 4> panelCorners(const VrLayout &layout, render::Vector2 gameSize)
{
    const render::Matrix transform = panelTransform(layout, gameSize, 1.0f);
    return {transformPoint(transform, {-0.5f, 0.5f, 0.0f}), transformPoint(transform, {0.5f, 0.5f, 0.0f}),
            transformPoint(transform, {0.5f, -0.5f, 0.0f}), transformPoint(transform, {-0.5f, -0.5f, 0.0f})};
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

render::Vector3 seatedEyePoint(const VrLayout &layout)
{
    return {CockpitEyePoint.x, CockpitEyePoint.y + layout.seatHeight, CockpitEyePoint.z};
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
