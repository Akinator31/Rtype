#pragma once

#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <string>

namespace rtype::render
{
class Renderer;
} // namespace rtype::render

namespace rtype::engine
{

/// Measures needed to place the interface: the size of a text, the size of the game, and where the mouse
/// pointer is in the game.
///
/// RendererMetrics gives the real ones. Tests use fake ones, so that the interface is tested without a
/// window.
class Metrics
{
public:
    virtual ~Metrics() = default;

    /// Size that a text takes once drawn, in game units.
    [[nodiscard]] virtual render::Vector2 measureText(render::FontId font, const std::string &text,
                                                      float size) const = 0;
    /// Logical resolution of the game.
    [[nodiscard]] virtual render::Vector2 gameSize() const = 0;
    /// Converts a position in the window, in pixels, into game units.
    [[nodiscard]] virtual render::Vector2 windowToGame(render::Vector2 position) const = 0;
};

/// The real measures, given by the Renderer.
class RendererMetrics : public Metrics
{
private:
    const render::Renderer &_renderer;

public:
    /// @param renderer It must outlive the metrics.
    explicit RendererMetrics(const render::Renderer &renderer) : _renderer(renderer) {}

    [[nodiscard]] render::Vector2 measureText(render::FontId font, const std::string &text,
                                              float size) const override;
    [[nodiscard]] render::Vector2 gameSize() const override;
    [[nodiscard]] render::Vector2 windowToGame(render::Vector2 position) const override;
};

} // namespace rtype::engine
