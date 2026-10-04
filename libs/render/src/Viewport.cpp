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

} // namespace rtype::render
