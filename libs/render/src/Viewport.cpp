#include "rtype/render/Viewport.hpp"

#include <algorithm>

namespace rtype::render
{

namespace
{

bool isEmpty(Vector2 size)
{
    return size.x <= 0.0f || size.y <= 0.0f;
}

} // namespace

Rectangle computeVisibleArea(Vector2 windowSize, Vector2 gameSize)
{
    if (isEmpty(windowSize) || isEmpty(gameSize)) {
        return {0.0f, 0.0f, gameSize.x, gameSize.y};
    }
    const float scale = std::min(windowSize.x / gameSize.x, windowSize.y / gameSize.y);
    const float width = windowSize.x / scale;
    const float height = windowSize.y / scale;
    return {(gameSize.x - width) / 2.0f, (gameSize.y - height) / 2.0f, width, height};
}

Vector2 windowToGame(Vector2 position, Vector2 windowSize, Vector2 gameSize)
{
    if (isEmpty(windowSize) || isEmpty(gameSize)) {
        return {};
    }
    const Rectangle area = computeVisibleArea(windowSize, gameSize);
    return {area.x + (position.x * area.width / windowSize.x),
            area.y + (position.y * area.height / windowSize.y)};
}

} // namespace rtype::render
