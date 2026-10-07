#include "rtype/render/Viewport.hpp"

#include <algorithm>

namespace rtype::render
{

Rectangle letterbox(Vector2 windowSize, Vector2 gameSize)
{
    if (windowSize.x <= 0.0f || windowSize.y <= 0.0f || gameSize.x <= 0.0f || gameSize.y <= 0.0f) {
        return {};
    }
    const float scale = std::min(windowSize.x / gameSize.x, windowSize.y / gameSize.y);
    const float width = gameSize.x * scale;
    const float height = gameSize.y * scale;
    return {(windowSize.x - width) / 2.0f, (windowSize.y - height) / 2.0f, width, height};
}

Vector2 windowToGame(Vector2 position, Vector2 windowSize, Vector2 gameSize)
{
    const Rectangle area = letterbox(windowSize, gameSize);
    if (area.width <= 0.0f || area.height <= 0.0f) {
        return {};
    }
    return {(position.x - area.x) * gameSize.x / area.width,
            (position.y - area.y) * gameSize.y / area.height};
}

} // namespace rtype::render
