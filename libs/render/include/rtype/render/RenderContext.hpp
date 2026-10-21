#pragma once

#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <string>

namespace rtype::render
{

class RenderContext
{
public:
    virtual ~RenderContext() = default;
    [[nodiscard]] virtual Vector2 gameSize() const = 0;
    [[nodiscard]] virtual Rectangle visibleArea() const = 0;
    [[nodiscard]] virtual Vector2 windowToGame(Vector2 position) const = 0;
    [[nodiscard]] virtual Vector2 textureSize(TextureId texture) const = 0;
    [[nodiscard]] virtual Vector2 measureText(FontId font, const std::string &text, float size) const = 0;
    virtual void drawTexture(TextureId texture, Vector2 position, Color tint = colors::White) = 0;
    virtual void drawTexture(TextureId texture, Rectangle source, Rectangle destination,
                             float rotation = 0.0f, Color tint = colors::White) = 0;
    virtual void drawRectangle(Rectangle rectangle, Color color) = 0;
    virtual void drawCircle(Vector2 center, float radius, Color color) = 0;
    virtual void drawRectangleOutline(Rectangle rectangle, float thickness, Color color) = 0;
    virtual void drawRoundedRectangle(Rectangle rectangle, float radius, Color left, Color right) = 0;
    virtual void drawRoundedRectangleOutline(Rectangle rectangle, float radius, float thickness,
                                             Color color) = 0;
    virtual void drawCircleOutline(Vector2 center, float radius, float thickness, Color color) = 0;
    virtual void drawLine(Vector2 start, Vector2 end, float thickness, Color color) = 0;
    virtual void drawTriangle(Vector2 first, Vector2 second, Vector2 third, Color color) = 0;
    virtual void drawModel(ModelId model, const Camera &camera, const Light &light, Vector3 position,
                           Vector3 rotationAxis = {0.0f, 1.0f, 0.0f}, float rotationAngle = 0.0f,
                           float scale = 1.0f, Color tint = colors::White, float spinAngle = 0.0f) = 0;
    virtual void animateModel(ModelId model, int animation, float time) = 0;
    virtual void blendModelAnimations(ModelId model, int from, float fromTime, int to, float toTime,
                                      float amount) = 0;
    virtual void drawText(FontId font, const std::string &text, Vector2 position, float size,
                          Color color) = 0;
};

} // namespace rtype::render
