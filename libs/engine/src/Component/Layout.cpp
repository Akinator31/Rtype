#include "rtype/engine/Component/Layout.hpp"

namespace rtype::engine
{

render::Vector2 getAnchorFraction(AnchorPoint point)
{
    switch (point) {
    case AnchorPoint::TopLeft:
        return {0.0f, 0.0f};
    case AnchorPoint::Top:
        return {0.5f, 0.0f};
    case AnchorPoint::TopRight:
        return {1.0f, 0.0f};
    case AnchorPoint::Left:
        return {0.0f, 0.5f};
    case AnchorPoint::Center:
        return {0.5f, 0.5f};
    case AnchorPoint::Right:
        return {1.0f, 0.5f};
    case AnchorPoint::BottomLeft:
        return {0.0f, 1.0f};
    case AnchorPoint::Bottom:
        return {0.5f, 1.0f};
    case AnchorPoint::BottomRight:
        return {1.0f, 1.0f};
    }
    return {0.5f, 0.5f};
}

render::Vector2 computeAnchoredPosition(render::Rectangle screen, const Anchor &anchor, render::Vector2 size)
{
    const render::Vector2 fraction = getAnchorFraction(anchor.point);
    // Elements are drawn centred on their position: move the centre away from the anchored point
    return {screen.x + (fraction.x * screen.width) + anchor.x + ((0.5f - fraction.x) * size.x),
            screen.y + (fraction.y * screen.height) + anchor.y + ((0.5f - fraction.y) * size.y)};
}

} // namespace rtype::engine
