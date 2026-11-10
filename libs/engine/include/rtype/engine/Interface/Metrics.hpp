#pragma once

#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <array>
#include <optional>
#include <string>

namespace rtype::render
{
class Renderer;
} // namespace rtype::render

namespace rtype::engine
{

/// Measures needed to place the interface: the size of a text, the size of the game and the part of it shown
/// on the screen, and where the mouse pointer is in the game.
///
/// RendererMetrics gives the real ones. Tests use fake ones, so that the interface is tested without a
/// window.
class Metrics
{
public:
    virtual ~Metrics() = default;

    /// Size that a text takes once drawn, in game units.
    /// @param spacing Space added between two characters (see render::Renderer::drawText()).
    [[nodiscard]] virtual render::Vector2 measureText(render::FontId font, const std::string &text,
                                                      float size, float spacing) const = 0;
    /// Size of the game, the same on every screen.
    [[nodiscard]] virtual render::Vector2 gameSize() const = 0;
    /// Part of the game shown on the screen: the game area, plus what is around it when the screen has
    /// another shape (see render::computeVisibleArea()). The interface follows its edges.
    [[nodiscard]] virtual render::Rectangle visibleArea() const = 0;
    /// Converts a position of the mouse, in pixels of the window, into game units. While the surfaces are
    /// apart (a VR headset shows the game), the pointer is a controller pointing at the panel, and its
    /// positions are already in game units of the panel: they are kept as they are.
    [[nodiscard]] virtual render::Vector2 windowToGame(render::Vector2 position) const = 0;
    /// Whether the surfaces of the interface are drawn apart, each into its image (a VR headset shows the
    /// game), or all on the screen (the default).
    [[nodiscard]] virtual bool surfacesApart() const { return false; }
    /// The area of a surface, in game units, where the anchors place its entities: the visible area when the
    /// surfaces are not apart.
    [[nodiscard]] virtual render::Rectangle surfaceArea(Surface /*surface*/) const { return visibleArea(); }
};

/// The area of each surface, by Surface.
using SurfaceAreas = std::array<render::Rectangle, SurfaceCount>;

/// The real measures, given by the Renderer.
class RendererMetrics : public Metrics
{
private:
    const render::Renderer &_renderer;
    std::optional<SurfaceAreas> _surfaces;

public:
    /// @param renderer It must outlive the metrics.
    explicit RendererMetrics(const render::Renderer &renderer) : _renderer(renderer) {}

    [[nodiscard]] render::Vector2 measureText(render::FontId font, const std::string &text, float size,
                                              float spacing) const override;
    [[nodiscard]] render::Vector2 gameSize() const override;
    [[nodiscard]] render::Rectangle visibleArea() const override;
    [[nodiscard]] render::Vector2 windowToGame(render::Vector2 position) const override;
    [[nodiscard]] bool surfacesApart() const override { return _surfaces.has_value(); }
    [[nodiscard]] render::Rectangle surfaceArea(Surface surface) const override;

    /// Draws the surfaces apart, each in its area, or all on the screen again (none). Apart, the visible area
    /// is the one of the panel: the frames drawn into the images do not move it; and the pointer is given in
    /// game units of the panel (windowToGame()).
    void separateSurfaces(std::optional<SurfaceAreas> areas) { _surfaces = areas; }
};

} // namespace rtype::engine
