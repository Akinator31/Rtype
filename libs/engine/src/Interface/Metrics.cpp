#include "rtype/engine/Interface/Metrics.hpp"

#include "rtype/render/Renderer.hpp"

namespace rtype::engine
{

render::Vector2 RendererMetrics::measureText(render::FontId font, const std::string &text, float size,
                                             float spacing) const
{
    return _renderer.measureText(font, text, size, spacing);
}

render::Vector2 RendererMetrics::gameSize() const
{
    return _renderer.gameSize();
}

render::Rectangle RendererMetrics::visibleArea() const
{
    if (_surfaces) {
        return (*_surfaces)[static_cast<std::size_t>(Surface::Panel)];
    }
    return _renderer.visibleArea();
}

render::Rectangle RendererMetrics::surfaceArea(Surface surface) const
{
    if (_surfaces) {
        return (*_surfaces)[static_cast<std::size_t>(surface)];
    }
    return visibleArea();
}

render::Vector2 RendererMetrics::windowToGame(render::Vector2 position) const
{
    // In the headset, the controllers point at the panel, in its game units: the window shows the mirror
    if (_surfaces) {
        return position;
    }
    return _renderer.windowToGame(position);
}

} // namespace rtype::engine
