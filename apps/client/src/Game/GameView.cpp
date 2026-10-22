#include "Game/GameView.hpp"

namespace rtype::client
{

namespace
{

/// How far in front of the plane of the game the camera is: further than the depth of any model.
constexpr float GameViewDistance = 1000.0f;

} // namespace

render::Camera computeGameView(render::Rectangle visibleArea)
{
    const float centreX = visibleArea.x + (visibleArea.width / 2.0f);
    const float centreY = visibleArea.y + (visibleArea.height / 2.0f);
    return {{centreX, centreY, -GameViewDistance},
            {centreX, centreY, 0.0f},
            {0.0f, -1.0f, 0.0f},
            visibleArea.height,
            render::Projection::Orthographic};
}

} // namespace rtype::client
