#include "rtype/render/Renderer.hpp"

#include "AntialiasPass.hpp"
#include "AuraShader.hpp"
#include "EnergyShader.hpp"
#include "FilterShader.hpp"
#include "GlowPass.hpp"
#include "LightingShader.hpp"
#include "Resources.hpp"
#include "rtype/render/Viewport.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <vector>

namespace rtype::render
{

namespace
{

// Size of raylib's built-in font: its spacing between characters is scaled from this size, like DrawText()
constexpr float builtinFontSize = 10.0f;

// How much of the light of emission maps (engine flames, windows, beacons) shines in the glow of the frame
constexpr float emissionBloom = 0.7f;
// Size of the picture of the vignette, stretched over the frame: smooth enough once filtered
constexpr int vignetteSize = 256;

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
    const int projection =
        camera.projection == Projection::Orthographic ? CAMERA_ORTHOGRAPHIC : CAMERA_PERSPECTIVE;
    return {toRaylib(camera.position), toRaylib(camera.target), toRaylib(camera.up), camera.fieldOfView,
            projection};
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

// Colour of a gradient at a place of it: the first colour before its first stop, the last one after its last
Color gradientAt(const std::vector<ColorStop> &stops, float position)
{
    if (position <= stops.front().position) {
        return stops.front().color;
    }
    for (std::size_t index = 1; index < stops.size(); ++index) {
        if (position <= stops[index].position) {
            const float span = stops[index].position - stops[index - 1].position;
            const float amount = span > 0.0f ? (position - stops[index - 1].position) / span : 1.0f;
            return mix(stops[index - 1].color, stops[index].color, amount);
        }
    }
    return stops.back().color;
}

// A glyph, from the top of its line to the bottom of it, drawn in strips so that each stop of the gradient
// is a corner: the colour changes linearly between two corners
void addGradientGlyph(const std::vector<ColorStop> &stops, ::Rectangle source, ::Rectangle destination,
                      float lineTop, float lineHeight, float textureWidth, float textureHeight)
{
    std::vector<float> cuts{destination.y, destination.y + destination.height};
    for (const ColorStop &stop : stops) {
        const float y = lineTop + (stop.position * lineHeight);
        if (y > destination.y && y < destination.y + destination.height) {
            cuts.push_back(y);
        }
    }
    std::ranges::sort(cuts);

    const float left = source.x / textureWidth;
    const float right = (source.x + source.width) / textureWidth;
    const auto textureV = [&](float y) {
        return (source.y + (((y - destination.y) / destination.height) * source.height)) / textureHeight;
    };
    const auto colorAt = [&](float y) {
        const Color color = gradientAt(stops, (y - lineTop) / lineHeight);
        rlColor4ub(color.r, color.g, color.b, color.a);
    };
    for (std::size_t index = 1; index < cuts.size(); ++index) {
        const float top = cuts[index - 1];
        const float bottom = cuts[index];
        colorAt(top);
        rlTexCoord2f(left, textureV(top));
        rlVertex2f(destination.x, top);
        colorAt(bottom);
        rlTexCoord2f(left, textureV(bottom));
        rlVertex2f(destination.x, bottom);
        colorAt(bottom);
        rlTexCoord2f(right, textureV(bottom));
        rlVertex2f(destination.x + destination.width, bottom);
        colorAt(top);
        rlTexCoord2f(right, textureV(top));
        rlVertex2f(destination.x + destination.width, top);
    }
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
            edge.push_back(
                {centres[corner].x + radius * std::cos(angle), centres[corner].y + radius * std::sin(angle)});
        }
    }
    return edge;
}

void addVertex(Vector2 position, Color color)
{
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlVertex2f(position.x, position.y);
}

// Owns the off-screen image the game is drawn to, at the resolution it is shown at in the window
class RenderTarget
{
private:
    RenderTexture2D _target;

public:
    explicit RenderTarget(Vector2 resolution)
        : _target(LoadRenderTexture(static_cast<int>(resolution.x), static_cast<int>(resolution.y)))
    {
        if (!IsRenderTextureValid(_target)) {
            throw std::runtime_error("Cannot create the render target");
        }
        SetTextureFilter(_target.texture, TEXTURE_FILTER_BILINEAR);
        // The smoothing of the edges reads the pixels around each one: none from the opposite edge
        SetTextureWrap(_target.texture, TEXTURE_WRAP_CLAMP);
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
    [[nodiscard]] Vector2 resolution() const
    {
        return {static_cast<float>(_target.texture.width), static_cast<float>(_target.texture.height)};
    }
};

Vector2 getWindowSize()
{
    return {static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())};
}

// The colour with its alpha multiplied by `opacity`, from 0 to 1
::Color fade(Color color, float opacity)
{
    const float alpha = static_cast<float>(color.a) * std::clamp(opacity, 0.0f, 1.0f);
    color.a = static_cast<std::uint8_t>(std::lround(alpha));
    return toRaylib(color);
}

// Where a point of a model ends up in the world when the model is drawn like DrawModelEx(), after turning by
// spinAngle around its own vertical axis
::Vector3 placePoint(Vector3 point, ::Vector3 position, ::Vector3 rotationAxis, float rotationAngle,
                     float spinAngle, float scale)
{
    ::Vector3 placed = Vector3RotateByAxisAngle(toRaylib(point), {0.0f, 1.0f, 0.0f}, spinAngle * DEG2RAD);
    placed = Vector3RotateByAxisAngle(Vector3Scale(placed, scale), rotationAxis, rotationAngle * DEG2RAD);
    return Vector3Add(placed, position);
}

// Draws the halo of an energy model, facing the camera, added to what is behind it. Neither hidden by nor
// hiding the models around: a flat picture crossing a ship would otherwise be cut along a straight line.
// Must be called in 3D mode.
void drawHalo(const Camera3D &camera, const Texture2D &texture, const Energy::Halo &halo, ::Vector3 centre,
              float scale, float opacity)
{
    const ::Vector2 size{halo.size.x * scale, halo.size.y * scale};
    const ::Rectangle source{0.0f, 0.0f, static_cast<float>(texture.width),
                             static_cast<float>(texture.height)};
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();
    rlDisableDepthTest();
    DrawBillboardPro(camera, texture, source, centre, camera.up, size, {size.x / 2.0f, size.y / 2.0f}, 0.0f,
                     fade(halo.color, opacity));
    // The billboard is batched: draw it before the depth test comes back
    rlDrawRenderBatchActive();
    rlEnableDepthTest();
    rlEnableDepthMask();
    EndBlendMode();
}

// The darkening of the corners of the frame (Renderer::setVignette()): black, transparent in the middle and
// more and more opaque towards the corners, more in them than along the edges
class Vignette
{
private:
    Texture2D _texture{};

public:
    Vignette()
    {
        Image image = GenImageColor(vignetteSize, vignetteSize, ::Color{0, 0, 0, 0});
        auto *pixels = static_cast<::Color *>(image.data);
        const auto toCentred = [](int pixel) {
            return ((static_cast<float>(pixel) + 0.5f) / static_cast<float>(vignetteSize) * 2.0f) - 1.0f;
        };
        for (int y = 0; y < vignetteSize; ++y) {
            for (int x = 0; x < vignetteSize; ++x) {
                // 0 in the middle, 1 in the corners
                const float u = toCentred(x);
                const float v = toCentred(y);
                const float distance = std::sqrt((u * u) + (v * v)) / std::numbers::sqrt2_v<float>;
                const float amount = std::clamp((distance - 0.45f) / 0.55f, 0.0f, 1.0f);
                const float smooth = amount * amount * (3.0f - (2.0f * amount));
                pixels[(y * vignetteSize) + x].a = static_cast<std::uint8_t>(std::lround(smooth * 255.0f));
            }
        }
        _texture = LoadTextureFromImage(image);
        UnloadImage(image);
        SetTextureFilter(_texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(_texture, TEXTURE_WRAP_CLAMP);
    }
    ~Vignette()
    {
        if (_texture.id > 0) {
            UnloadTexture(_texture);
        }
    }

    Vignette(const Vignette &) = delete;
    Vignette &operator=(const Vignette &) = delete;
    Vignette(Vignette &&) = delete;
    Vignette &operator=(Vignette &&) = delete;

    // Darkens the corners of an image of the given resolution, drawn in its pixels, by up to `strength`
    void draw(Vector2 resolution, float strength) const
    {
        if (_texture.id == 0 || strength <= 0.0f) {
            return;
        }
        const ::Rectangle source{0.0f, 0.0f, static_cast<float>(_texture.width),
                                 static_cast<float>(_texture.height)};
        DrawTexturePro(_texture, source, {0.0f, 0.0f, resolution.x, resolution.y}, {0.0f, 0.0f}, 0.0f,
                       fade(colors::White, strength));
    }
};

} // namespace

struct Renderer::Impl {
    /// A model drawn in the frame that shines, to draw again in its glow (drawGlow()): an energy model, the
    /// aura of a model, or a lit model with an emission map
    struct GlowDraw {
        ModelId model;
        Camera3D camera;
        std::optional<Energy> energy;
        std::optional<Aura> aura;
        ::Vector3 position;
        ::Vector3 rotationAxis;
        float rotationAngle;
        float scale;
        float spinAngle;
        ::Color tint;
        std::optional<detail::Model::Pose> pose;
        std::vector<int> hiddenMeshes;
    };

    const core::ResourceCache<detail::Texture, TextureId> &textures;
    const core::ResourceCache<detail::Font, FontId> &fonts;
    const core::ResourceCache<detail::Model, ModelId> &models;
    Vector2 gameSize;
    Rectangle frameArea;
    std::optional<RenderTarget> target;
    detail::AntialiasPass antialiasPass;
    detail::FilterShader filterShader;
    detail::LightingShader lightingShader;
    detail::EnergyShader energyShader;
    detail::AuraShader auraShader;
    detail::GlowPass glowPass;
    Vignette vignette;
    std::vector<GlowDraw> glowQueue;
    ColorFilter colorFilter = ColorFilter::None;
    float vignetteStrength = 0.22f;
    bool antialiasing = true;
    bool inFrame = false;

    Impl(const core::ResourceCache<detail::Texture, TextureId> &textureCache,
         const core::ResourceCache<detail::Font, FontId> &fontCache,
         const core::ResourceCache<detail::Model, ModelId> &modelCache, Vector2 size)
        : textures(textureCache), fonts(fontCache), models(modelCache), gameSize(size),
          frameArea{0.0f, 0.0f, size.x, size.y}
    {
        fitTargetToWindow();
    }

    [[nodiscard]] Rectangle getVisibleArea() const { return computeVisibleArea(getWindowSize(), gameSize); }

    void requireFrame() const
    {
        if (!inFrame) {
            throw std::logic_error("Drawing outside of a frame: call beginFrame() first");
        }
    }

    /// Recreates the render target when the window changed size, so that the image is drawn at the resolution
    /// of the window and never stretched. A minimised window keeps the previous target.
    void fitTargetToWindow()
    {
        frameArea = getVisibleArea();
        const Vector2 window = getWindowSize();
        Vector2 resolution{std::round(window.x), std::round(window.y)};
        if (resolution.x < 1.0f || resolution.y < 1.0f) {
            if (target) {
                return;
            }
            resolution = gameSize;
        }
        if (!target || target->resolution().x != resolution.x || target->resolution().y != resolution.y) {
            target.reset();
            target.emplace(resolution);
        }
    }

    /// Draws a model again in the glow of the frame: an energy model as it is, a lit model only where it
    /// emits light. Must be called in the images of the glow (GlowPass::render()).
    void drawInGlow(const GlowDraw &draw)
    {
        const detail::Model &drawn = models.get(draw.model);
        if (draw.pose) {
            drawn.applyPose(*draw.pose);
        }
        BeginMode3D(draw.camera);
        if (draw.aura) {
            auraShader.draw(drawn.get(), *draw.aura, draw.camera, draw.position, draw.rotationAxis,
                            draw.rotationAngle, draw.spinAngle, draw.scale, draw.hiddenMeshes,
                            draw.aura->bloom);
        } else if (draw.energy) {
            energyShader.draw(drawn.get(), *draw.energy, textures.get(draw.energy->noise).get(),
                              draw.camera.position, draw.position, draw.rotationAxis, draw.rotationAngle,
                              draw.spinAngle, draw.scale, draw.tint);
        } else {
            lightingShader.drawEmission(drawn.get(), draw.camera, draw.position, draw.rotationAxis,
                                        draw.rotationAngle, draw.spinAngle, draw.scale, draw.tint,
                                        draw.hiddenMeshes);
        }
        EndMode3D();
    }

    /// Makes the visible area of the game cover the render target: positions and sizes stay in game units
    /// whatever its resolution.
    void beginGameView() const
    {
        const float zoom = target->resolution().x / frameArea.width;
        BeginMode2D(Camera2D{
            .offset = {0.0f, 0.0f}, .target = {frameArea.x, frameArea.y}, .rotation = 0.0f, .zoom = zoom});
    }
};

// The window is only required to make sure that one is open: the render target needs it
Renderer::Renderer(const Window & /*window*/, const ResourceManager &resources, Vector2 gameSize)
{
    if (gameSize.x <= 0.0f || gameSize.y <= 0.0f) {
        throw std::invalid_argument("The game size must be strictly positive");
    }
    _impl = std::make_unique<Impl>(resources._impl->textures, resources._impl->fonts, resources._impl->models,
                                   gameSize);
}

Renderer::~Renderer() = default;
Renderer::Renderer(Renderer &&) noexcept = default;
Renderer &Renderer::operator=(Renderer &&) noexcept = default;

Vector2 Renderer::gameSize() const
{
    return _impl->gameSize;
}

Rectangle Renderer::visibleArea() const
{
    return _impl->getVisibleArea();
}

void Renderer::setColorFilter(ColorFilter filter)
{
    _impl->colorFilter = filter;
}

ColorFilter Renderer::colorFilter() const
{
    return _impl->colorFilter;
}

void Renderer::setVignette(float strength)
{
    _impl->vignetteStrength = std::clamp(strength, 0.0f, 1.0f);
}

float Renderer::vignette() const
{
    return _impl->vignetteStrength;
}

void Renderer::setAntialiasing(bool enabled)
{
    _impl->antialiasing = enabled;
}

bool Renderer::antialiasing() const
{
    return _impl->antialiasing;
}

Vector2 Renderer::windowToGame(Vector2 position) const
{
    return render::windowToGame(position, getWindowSize(), _impl->gameSize);
}

void Renderer::beginFrame(Color background)
{
    if (_impl->inFrame) {
        throw std::logic_error("A frame is already started: call endFrame() first");
    }
    _impl->fitTargetToWindow();
    _impl->glowQueue.clear();
    BeginTextureMode(_impl->target->get());
    ClearBackground(toRaylib(background));
    _impl->beginGameView();
    _impl->inFrame = true;
}

void Renderer::endFrame()
{
    if (!_impl->inFrame) {
        throw std::logic_error("No frame to end: call beginFrame() first");
    }
    EndMode2D();
    EndTextureMode();
    _impl->inFrame = false;

    const Texture2D &image = _impl->target->get().texture;
    const Vector2 windowSize = getWindowSize();
    const ::Rectangle source{0.0f, 0.0f, static_cast<float>(image.width), -static_cast<float>(image.height)};

    BeginDrawing();
    ClearBackground(toRaylib(colors::Black));

    const bool filtered = _impl->filterShader.begin(_impl->colorFilter);

    DrawTexturePro(image, source, {0.0f, 0.0f, windowSize.x, windowSize.y}, {0.0f, 0.0f}, 0.0f,
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

void Renderer::drawGradientRectangle(Rectangle rectangle, Color top, Color bottom)
{
    _impl->requireFrame();
    // The corners go anticlockwise from the top left
    DrawRectangleGradientEx(toRaylib(rectangle), toRaylib(top), toRaylib(bottom), toRaylib(bottom),
                            toRaylib(top));
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

void Renderer::drawArc(Vector2 center, float radius, float thickness, float startAngle, float endAngle,
                       Color color)
{
    _impl->requireFrame();
    if (thickness > 0.0f && radius > 0.0f && endAngle > startAngle) {
        constexpr float degreesPerSegment = 4.0f;
        const int segments =
            std::max(2, static_cast<int>(std::ceil((endAngle - startAngle) / degreesPerSegment)));
        DrawRing(toRaylib(center), std::max(radius - thickness, 0.0f), radius, startAngle, endAngle, segments,
                 toRaylib(color));
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
                         Vector3 rotationAxis, float rotationAngle, float scale, Color tint, float spinAngle,
                         const std::vector<int> &hiddenMeshes)
{
    _impl->requireFrame();
    const Camera3D view = toRaylib(camera);
    const detail::Model &drawn = _impl->models.get(model);
    BeginMode3D(view);
    _impl->lightingShader.draw(drawn.get(), light, view, toRaylib(position), toRaylib(rotationAxis),
                               rotationAngle, spinAngle, scale, toRaylib(tint), hiddenMeshes);
    // EndMode3D() resets the view to window pixels: go back to game units for what is drawn next
    EndMode3D();
    _impl->beginGameView();
    if (detail::LightingShader::hasEmission(drawn.get())) {
        _impl->glowQueue.push_back({model, view, std::nullopt, std::nullopt, toRaylib(position),
                                    toRaylib(rotationAxis), rotationAngle, scale, spinAngle,
                                    fade(tint, emissionBloom), drawn.getPose(), hiddenMeshes});
    }
}

void Renderer::drawEnergyModel(ModelId model, const Camera &camera, const Energy &energy, Vector3 position,
                               Vector3 rotationAxis, float rotationAngle, float scale, Color tint,
                               float spinAngle)
{
    _impl->requireFrame();
    const Camera3D view = toRaylib(camera);
    const ::Vector3 origin = toRaylib(position);
    const ::Vector3 axis = toRaylib(rotationAxis);
    BeginMode3D(view);
    _impl->energyShader.draw(_impl->models.get(model).get(), energy, _impl->textures.get(energy.noise).get(),
                             view.position, origin, axis, rotationAngle, spinAngle, scale, toRaylib(tint));
    if (energy.halo) {
        const ::Vector3 centre =
            placePoint(energy.halo->centre, origin, axis, rotationAngle, spinAngle, scale);
        drawHalo(view, _impl->textures.get(energy.halo->texture).get(), *energy.halo, centre, scale,
                 static_cast<float>(tint.a) / 255.0f);
    }
    EndMode3D();
    _impl->beginGameView();
    if (energy.bloom > 0.0f) {
        _impl->glowQueue.push_back({model,
                                    view,
                                    energy,
                                    std::nullopt,
                                    origin,
                                    axis,
                                    rotationAngle,
                                    scale,
                                    spinAngle,
                                    fade(tint, energy.bloom),
                                    _impl->models.get(model).getPose(),
                                    {}});
    }
}

void Renderer::drawAura(ModelId model, const Camera &camera, const Aura &aura, Vector3 position,
                        Vector3 rotationAxis, float rotationAngle, float scale, float spinAngle,
                        const std::vector<int> &hiddenMeshes)
{
    _impl->requireFrame();
    const Camera3D view = toRaylib(camera);
    const ::Vector3 origin = toRaylib(position);
    const ::Vector3 axis = toRaylib(rotationAxis);
    const detail::Model &drawn = _impl->models.get(model);
    BeginMode3D(view);
    _impl->auraShader.draw(drawn.get(), aura, view, origin, axis, rotationAngle, spinAngle, scale,
                           hiddenMeshes);
    EndMode3D();
    _impl->beginGameView();
    if (aura.bloom > 0.0f) {
        _impl->glowQueue.push_back({model, view, std::nullopt, aura, origin, axis, rotationAngle, scale,
                                    spinAngle, toRaylib(colors::White), drawn.getPose(), hiddenMeshes});
    }
}

void Renderer::drawGlow()
{
    _impl->requireFrame();
    const Vector2 resolution = _impl->target->resolution();
    // The smoothing, the glow and the vignette are drawn in pixels of the frame, the first two in images of
    // their own: leave the game view and the frame, then come back to them, keeping what the frame holds
    EndMode2D();
    EndTextureMode();
    if (_impl->antialiasing) {
        _impl->antialiasPass.apply(_impl->target->get());
    }
    const Texture2D *glow = nullptr;
    if (!_impl->glowQueue.empty()) {
        glow = _impl->glowPass.render(toRaylib(resolution), [this] {
            // The lit models first: they write their depth, so that they hide the energy and the auras behind
            // them
            for (const bool added : {false, true}) {
                for (const Impl::GlowDraw &draw : _impl->glowQueue) {
                    if ((draw.energy.has_value() || draw.aura.has_value()) == added) {
                        _impl->drawInGlow(draw);
                    }
                }
            }
        });
        _impl->glowQueue.clear();
    }

    BeginTextureMode(_impl->target->get());
    if (glow != nullptr) {
        BeginBlendMode(BLEND_ADDITIVE);
        const auto width = static_cast<float>(glow->width);
        const auto height = static_cast<float>(glow->height);
        DrawTexturePro(*glow, {0.0f, 0.0f, width, -height}, {0.0f, 0.0f, resolution.x, resolution.y},
                       {0.0f, 0.0f}, 0.0f, toRaylib(colors::White));
        EndBlendMode();
    }
    _impl->vignette.draw(resolution, _impl->vignetteStrength);
    _impl->beginGameView();
}

void Renderer::animateModel(ModelId model, int animation, float time)
{
    _impl->models.get(model).animate(animation, time);
}

void Renderer::blendModelAnimations(ModelId model, int from, float fromTime, int to, float toTime,
                                    float amount)
{
    _impl->models.get(model).blendAnimations(from, fromTime, to, toTime, amount);
}

void Renderer::poseModel(ModelId model, const ModelPose &pose)
{
    ModelPose clamped = pose;
    clamped.amount = std::clamp(pose.amount, 0.0f, 1.0f);
    _impl->models.get(model).applyPose(clamped);
}

void Renderer::drawText(FontId font, const std::string &text, Vector2 position, float size, Color color,
                        float spacing)
{
    _impl->requireFrame();
    DrawTextEx(_impl->fonts.get(font).get(), text.c_str(), toRaylib(position), size, spacing,
               toRaylib(color));
}

void Renderer::drawGradientText(FontId font, const std::string &text, Vector2 position, float size,
                                const std::vector<ColorStop> &stops, float spacing)
{
    _impl->requireFrame();
    if (stops.empty() || text.empty()) {
        return;
    }
    // The glyphs are placed as DrawTextEx() does, but each one is drawn with a colour per corner
    const ::Font &face = _impl->fonts.get(font).get();
    const float scale = size / static_cast<float>(face.baseSize);
    const float padding = static_cast<float>(face.glyphPadding);
    const auto textureWidth = static_cast<float>(face.texture.width);
    const auto textureHeight = static_cast<float>(face.texture.height);
    float offset = 0.0f;

    rlSetTexture(face.texture.id);
    rlBegin(RL_QUADS);
    rlNormal3f(0.0f, 0.0f, 1.0f);
    for (std::size_t byte = 0; byte < text.size();) {
        int length = 0;
        const int codepoint = GetCodepointNext(text.c_str() + byte, &length);
        byte += static_cast<std::size_t>(std::max(length, 1));
        const int index = GetGlyphIndex(face, codepoint);
        const ::Rectangle &rectangle = face.recs[index];
        const ::GlyphInfo &glyph = face.glyphs[index];
        if (codepoint != ' ' && codepoint != '\t') {
            const ::Rectangle source{rectangle.x - padding, rectangle.y - padding,
                                     rectangle.width + (2.0f * padding), rectangle.height + (2.0f * padding)};
            const ::Rectangle destination{
                position.x + offset + ((static_cast<float>(glyph.offsetX) - padding) * scale),
                position.y + ((static_cast<float>(glyph.offsetY) - padding) * scale), source.width * scale,
                source.height * scale};
            addGradientGlyph(stops, source, destination, position.y, size, textureWidth, textureHeight);
        }
        const float advance =
            glyph.advanceX == 0 ? rectangle.width * scale : static_cast<float>(glyph.advanceX) * scale;
        offset += advance + spacing;
    }
    rlEnd();
    rlSetTexture(0);
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

Vector2 Renderer::measureText(FontId font, const std::string &text, float size, float spacing) const
{
    return fromRaylib(MeasureTextEx(_impl->fonts.get(font).get(), text.c_str(), size, spacing));
}

Vector2 Renderer::measureText(const std::string &text, float size) const
{
    return fromRaylib(MeasureTextEx(GetFontDefault(), text.c_str(), size, builtinFontSpacing(size)));
}

} // namespace rtype::render
