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
    return _renderer.visibleArea();
}

render::Vector2 RendererMetrics::windowToGame(render::Vector2 position) const
{
    return _renderer.windowToGame(position);
}

} // namespace rtype::engine
