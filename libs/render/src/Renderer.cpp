#include "rtype/render/Renderer.hpp"

#include "Resources.hpp"
#include "rtype/render/Viewport.hpp"

#include <raylib.h>

#include <stdexcept>

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

Vector2 fromRaylib(::Vector2 vector)
{
    return {vector.x, vector.y};
}

float builtinFontSpacing(float size)
{
    return size / builtinFontSize;
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
    const ResourceCache<detail::Texture, TextureId> &textures;
    const ResourceCache<detail::Font, FontId> &fonts;
    Vector2 gameSize;
    RenderTarget target;
    bool inFrame = false;

    Impl(const ResourceCache<detail::Texture, TextureId> &textureCache,
         const ResourceCache<detail::Font, FontId> &fontCache, Vector2 size)
        : textures(textureCache), fonts(fontCache), gameSize(size), target(size)
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
    _impl = std::make_unique<Impl>(resources._impl->textures, resources._impl->fonts, gameSize);
}

Renderer::~Renderer() = default;
Renderer::Renderer(Renderer &&) noexcept = default;
Renderer &Renderer::operator=(Renderer &&) noexcept = default;

Vector2 Renderer::gameSize() const
{
    return _impl->gameSize;
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
    DrawTexturePro(image, source, toRaylib(letterbox(windowSize, _impl->gameSize)), {0.0f, 0.0f}, 0.0f,
                   toRaylib(colors::White));
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
