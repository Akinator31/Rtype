#include "Vr/VrView.hpp"

namespace rtype::client
{

render::Matrix boardTransform(const BoardPlacement &placement, render::Vector2 gameSize)
{
    const float height = gameSize.x > 0.0f ? placement.width * gameSize.y / gameSize.x : 0.0f;
    render::Matrix transform;
    // Scaled, then moved: no rotation, the square already faces the eyes (+z)
    transform.values[0] = placement.width;
    transform.values[5] = height;
    transform.values[12] = 0.0f;
    transform.values[13] = -placement.drop;
    transform.values[14] = -placement.distance;
    return transform;
}

render::EyeView toRenderEye(const xr::EyeView &eye)
{
    render::EyeView view;
    view.view.values = eye.view.values;
    view.projection.values = eye.projection.values;
    view.position = {eye.position.x, eye.position.y, eye.position.z};
    view.width = eye.width;
    view.height = eye.height;
    return view;
}

VrView::VrView(render::Renderer &renderer, BoardPlacement placement)
    : _renderer(renderer), _placement(placement)
{
}

render::ImageId VrView::board()
{
    if (!_board) {
        _board = _renderer.createImage(BoardResolution);
    }
    return *_board;
}

void VrView::drawEye(int eye, const xr::EyeView &view)
{
    _renderer.beginEye(eye, toRenderEye(view));
    if (_board) {
        _renderer.drawImageQuad(*_board, boardTransform(_placement, _renderer.gameSize()));
    }
    _renderer.endEye();
}

} // namespace rtype::client
