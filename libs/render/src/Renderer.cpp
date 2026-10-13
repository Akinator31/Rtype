#include "rtype/render/Renderer.hpp"

#include "FilterShader.hpp"
#include "LightingShader.hpp"
#include "Resources.hpp"
#include "rtype/render/Viewport.hpp"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace rtype::render
{

namespace
{

// Size of raylib's built-in font: its spacing between characters is scaled from this size, like DrawText()
constexpr float builtinFontSize = 10.0f;

// Note: raylib colour macros (BLACK, WHITE...) cannot be used in this namespace, where Color is our type

::Vector2 toRaylib(Vector2 vector)
{
    return {vector.x, vector.y};
}

::Rectangle toRaylib(Rectangle rectangle)
{
    return {rectangle.x, rectangle.y, rectangle.width, rectangle.height};
}

::Color toRaylib(Color color)
{
    return {color.r, color.g, color.b, color.a};
}

::Vector3 toRaylib(Vector3 vector)
{
    return {vector.x, vector.y, vector.z};
}

Camera3D toRaylib(const Camera &camera)
{
    return {toRaylib(camera.position), toRaylib(camera.target), toRaylib(camera.up), camera.fieldOfView,
            CAMERA_PERSPECTIVE};
}

Vector2 fromRaylib(::Vector2 vector)
{
    return {vector.x, vector.y};
}

float builtinFontSpacing(float size)
{
    return size / builtinFontSize;
}

// Tells whether the corners are in the order raylib draws: counter-clockwise on screen. With the y axis
// pointing down, it is the order for which this cross product is negative.
bool isCounterClockwise(Vector2 first, Vector2 second, Vector2 third)
{
    const float cross =
        (second.x - first.x) * (third.y - first.y) - (second.y - first.y) * (third.x - first.x);
    return cross < 0.0f;
}

// Number of segments of each rounded corner: enough for the corners of a button to look round
constexpr int cornerSegments = 12;

Color mix(Color from, Color to, float amount)
{
    const auto channel = [amount](std::uint8_t start, std::uint8_t end) {
        return static_cast<std::uint8_t>(std::lround(
            static_cast<float>(start) + (static_cast<float>(end) - static_cast<float>(start)) * amount));
    };
    return {channel(from.r, to.r), channel(from.g, to.g), channel(from.b, to.b), channel(from.a, to.a)};
}

// Points along the edge of a rounded rectangle, clockwise on screen from the left end of the top-left corner.
// Every rectangle gets the same number of points, so that two edges can be joined point by point.
std::vector<Vector2> getRoundedEdge(Rectangle rectangle, float radius)
{
    radius = std::clamp(radius, 0.0f, std::min(rectangle.width, rectangle.height) / 2.0f);
    const float left = rectangle.x + radius;
    const float top = rectangle.y + radius;
    const float right = rectangle.x + rectangle.width - radius;
    const float bottom = rectangle.y + rectangle.height - radius;
    const std::array<Vector2, 4> centres{{{left, top}, {right, top}, {right, bottom}, {left, bottom}}};
    const std::array<float, 4> startAngles{std::numbers::pi_v<float>, 1.5f * std::numbers::pi_v<float>, 0.0f,
                                           0.5f * std::numbers::pi_v<float>};

    std::vector<Vector2> edge;
    edge.reserve(centres.size() * (cornerSegments + 1));
    for (std::size_t corner = 0; corner < centres.size(); ++corner) {
        for (int step = 0; step <= cornerSegments; ++step) {
            const float angle = startAngles[corner] + 0.5f * std::numbers::pi_v<float> *
                                                          static_cast<float>(step) /
                                                          static_cast<float>(cornerSegments);
            edge.push_back({centres[corner].x + radius * std::cos(angle),
                            centres[corner].y + radius * std::sin(angle)});
        }
    }
    return edge;
}

void addVertex(Vector2 position, Color color)
{
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlVertex2f(position.x, position.y);
}

// Owns the off-screen image the game is drawn to, at the game's logical resolution
class RenderTarget
{
private:
    RenderTexture2D _target;

public:
    explicit RenderTarget(Vector2 size)
        : _target(LoadRenderTexture(static_cast<int>(size.x), static_cast<int>(size.y)))
    {
        if (!IsRenderTextureValid(_target)) {
            throw std::runtime_error("Cannot create the render target");
        }
        SetTextureFilter(_target.texture, TEXTURE_FILTER_BILINEAR);
    }
    ~RenderTarget()
    {
        if (IsRenderTextureValid(_target)) {
            UnloadRenderTexture(_target);
        }
    }

    RenderTarget(const RenderTarget &) = delete;
    RenderTarget &operator=(const RenderTarget &) = delete;
    RenderTarget(RenderTarget &&) = delete;
    RenderTarget &operator=(RenderTarget &&) = delete;

    [[nodiscard]] const RenderTexture2D &get() const { return _target; }
};

} // namespace

struct Renderer::Impl {
    const core::ResourceCache<detail::Texture, TextureId> &textures;
    const core::ResourceCache<detail::Font, FontId> &fonts;
    const core::ResourceCache<detail::Model, ModelId> &models;
    Vector2 gameSize;
    RenderTarget target;
    detail::FilterShader filterShader;
    detail::LightingShader lightingShader;
    ColorFilter colorFilter = ColorFilter::None;
    bool inFrame = false;

    Impl(const core::ResourceCache<detail::Texture, TextureId> &textureCache,
         const core::ResourceCache<detail::Font, FontId> &fontCache,
         const core::ResourceCache<detail::Model, ModelId> &modelCache, Vector2 size)
        : textures(textureCache), fonts(fontCache), models(modelCache), gameSize(size), target(size)
    {
    }

    void requireFrame() const
    {
        if (!inFrame) {
            throw std::logic_error("Drawing outside of a frame: call beginFrame() first");
        }
    }
};

// The window is only required to make sure that one is open: the render target needs it
Renderer::Renderer(const Window & /*window*/, const ResourceManager &resources, Vector2 gameSize)
{
    if (gameSize.x <= 0.0f || gameSize.y <= 0.0f) {
        throw std::invalid_argument("The game size must be strictly positive");
    }
    _impl = std::make_unique<Impl>(resources._impl->textures, resources._impl->fonts,
                                     resources._impl->models, gameSize);
}

Renderer::~Renderer() = default;
Renderer::Renderer(Renderer &&) noexcept = default;
Renderer &Renderer::operator=(Renderer &&) noexcept = default;

Vector2 Renderer::gameSize() const
{
    return _impl->gameSize;
}

void Renderer::setColorFilter(ColorFilter filter)
{
    _impl->colorFilter = filter;
}

ColorFilter Renderer::colorFilter() const
{
    return _impl->colorFilter;
}

Vector2 Renderer::windowToGame(Vector2 position) const
{
    const Vector2 windowSize{static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())};
    return render::windowToGame(position, windowSize, _impl->gameSize);
}

void Renderer::beginFrame(Color background)
{
    if (_impl->inFrame) {
        throw std::logic_error("A frame is already started: call endFrame() first");
    }
    BeginTextureMode(_impl->target.get());
    ClearBackground(toRaylib(background));
    _impl->inFrame = true;
}

void Renderer::endFrame()
{
    if (!_impl->inFrame) {
        throw std::logic_error("No frame to end: call beginFrame() first");
    }
    EndTextureMode();
    _impl->inFrame = false;

    const Texture2D &image = _impl->target.get().texture;
    const Vector2 windowSize{static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())};
    // Render textures are stored upside down (OpenGL convention): a negative source height flips them back
    const ::Rectangle source{0.0f, 0.0f, static_cast<float>(image.width), -static_cast<float>(image.height)};

    BeginDrawing();
    ClearBackground(toRaylib(colors::Black));
    // The colour filter is applied to the whole frame while it is copied to the window
    const bool filtered = _impl->filterShader.begin(_impl->colorFilter);
    DrawTexturePro(image, source, toRaylib(letterbox(windowSize, _impl->gameSize)), {0.0f, 0.0f}, 0.0f,
                   toRaylib(colors::White));
    if (filtered) {
        EndShaderMode();
    }
    EndDrawing();
}

void Renderer::drawTexture(TextureId texture, Vector2 position, Color tint)
{
    _impl->requireFrame();
    DrawTextureV(_impl->textures.get(texture).get(), toRaylib(position), toRaylib(tint));
}

void Renderer::drawTexture(TextureId texture, Rectangle source, Rectangle destination, float rotation,
                           Color tint)
{
    _impl->requireFrame();
    // raylib rotates around the origin, given relative to the destination: use the centre and move the
    // destination by the same amount so that the texture stays where it was asked
    const ::Vector2 origin{destination.width / 2.0f, destination.height / 2.0f};
    destination.x += origin.x;
    destination.y += origin.y;
    DrawTexturePro(_impl->textures.get(texture).get(), toRaylib(source), toRaylib(destination), origin,
                   rotation, toRaylib(tint));
}

void Renderer::drawRectangle(Rectangle rectangle, Color color)
{
    _impl->requireFrame();
    DrawRectangleRec(toRaylib(rectangle), toRaylib(color));
}

void Renderer::drawCircle(Vector2 center, float radius, Color color)
{
    _impl->requireFrame();
    if (radius > 0.0f) {
        DrawCircleV(toRaylib(center), radius, toRaylib(color));
    }
}

void Renderer::drawRectangleOutline(Rectangle rectangle, float thickness, Color color)
{
    _impl->requireFrame();
    if (thickness > 0.0f && rectangle.width > 0.0f && rectangle.height > 0.0f) {
        // raylib draws the outline inside the rectangle, and fills it if the outline is thicker than half of
        // it
        DrawRectangleLinesEx(toRaylib(rectangle), thickness, toRaylib(color));
    }
}

void Renderer::drawRoundedRectangle(Rectangle rectangle, float radius, Color left, Color right)
{
    _impl->requireFrame();
    if (rectangle.width <= 0.0f || rectangle.height <= 0.0f) {
        return;
    }
    const auto colorAt = [&](Vector2 point) {
        return mix(left, right, (point.x - rectangle.x) / rectangle.width);
    };
    const Vector2 centre{rectangle.x + rectangle.width / 2.0f, rectangle.y + rectangle.height / 2.0f};
    const std::vector<Vector2> edge = getRoundedEdge(rectangle, radius);

    rlBegin(RL_TRIANGLES);
    for (std::size_t index = 0; index < edge.size(); ++index) {
        const Vector2 next = edge[(index + 1) % edge.size()];
        addVertex(centre, colorAt(centre));
        addVertex(next, colorAt(next));
        addVertex(edge[index], colorAt(edge[index]));
    }
    rlEnd();
}

void Renderer::drawRoundedRectangleOutline(Rectangle rectangle, float radius, float thickness, Color color)
{
    _impl->requireFrame();
    if (thickness <= 0.0f || rectangle.width <= 0.0f || rectangle.height <= 0.0f) {
        return;
    }
    thickness = std::min(thickness, std::min(rectangle.width, rectangle.height) / 2.0f);
    const std::vector<Vector2> outer = getRoundedEdge(rectangle, radius);
    const std::vector<Vector2> inner =
        getRoundedEdge({rectangle.x + thickness, rectangle.y + thickness, rectangle.width - 2.0f * thickness,
                        rectangle.height - 2.0f * thickness},
                       radius - thickness);

    rlBegin(RL_TRIANGLES);
    for (std::size_t index = 0; index < outer.size(); ++index) {
        const std::size_t next = (index + 1) % outer.size();
        addVertex(outer[index], color);
        addVertex(inner[index], color);
        addVertex(outer[next], color);
        addVertex(outer[next], color);
        addVertex(inner[index], color);
        addVertex(inner[next], color);
    }
    rlEnd();
}

void Renderer::drawCircleOutline(Vector2 center, float radius, float thickness, Color color)
{
    _impl->requireFrame();
    if (thickness > 0.0f && radius > 0.0f) {
        // A ring between the radius and the radius minus the thickness; 0 segments lets raylib pick enough of
        // them for the circle to look round
        const float innerRadius = std::max(radius - thickness, 0.0f);
        DrawRing(toRaylib(center), innerRadius, radius, 0.0f, 360.0f, 0, toRaylib(color));
    }
}

void Renderer::drawLine(Vector2 start, Vector2 end, float thickness, Color color)
{
    _impl->requireFrame();
    DrawLineEx(toRaylib(start), toRaylib(end), thickness, toRaylib(color));
}

void Renderer::drawTriangle(Vector2 first, Vector2 second, Vector2 third, Color color)
{
    _impl->requireFrame();
    // raylib silently skips triangles given clockwise (their back face is culled): put the corners in its
    // order
    if (!isCounterClockwise(first, second, third)) {
        std::swap(second, third);
    }
    DrawTriangle(toRaylib(first), toRaylib(second), toRaylib(third), toRaylib(color));
}

void Renderer::drawModel(ModelId model, const Camera &camera, const Light &light, Vector3 position,
                         Vector3 rotationAxis, float rotationAngle, float scale, Color tint)
{
    _impl->requireFrame();
    BeginMode3D(toRaylib(camera));
    _impl->lightingShader.draw(_impl->models.get(model).get(), light, toRaylib(position),
                               toRaylib(rotationAxis), rotationAngle, scale, toRaylib(tint));
    EndMode3D();
}

void Renderer::drawText(FontId font, const std::string &text, Vector2 position, float size, Color color)
{
    _impl->requireFrame();
    DrawTextEx(_impl->fonts.get(font).get(), text.c_str(), toRaylib(position), size, 0.0f, toRaylib(color));
}

void Renderer::drawText(const std::string &text, Vector2 position, float size, Color color)
{
    _impl->requireFrame();
    DrawTextEx(GetFontDefault(), text.c_str(), toRaylib(position), size, builtinFontSpacing(size),
               toRaylib(color));
}

void Renderer::drawFps(Vector2 position)
{
    _impl->requireFrame();
    DrawFPS(static_cast<int>(position.x), static_cast<int>(position.y));
}

Vector2 Renderer::textureSize(TextureId texture) const
{
    const Texture2D &raw = _impl->textures.get(texture).get();
    return {static_cast<float>(raw.width), static_cast<float>(raw.height)};
}

Vector2 Renderer::measureText(FontId font, const std::string &text, float size) const
{
    return fromRaylib(MeasureTextEx(_impl->fonts.get(font).get(), text.c_str(), size, 0.0f));
}

Vector2 Renderer::measureText(const std::string &text, float size) const
{
    return fromRaylib(MeasureTextEx(GetFontDefault(), text.c_str(), size, builtinFontSpacing(size)));
}

} // namespace rtype::render
