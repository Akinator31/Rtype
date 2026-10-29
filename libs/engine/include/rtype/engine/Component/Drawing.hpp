#pragma once

#include "rtype/render/Energy.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

/// Client side of the engine: components and systems that connect the ECS to the rendering, the input and the
/// interface. The ECS itself (rtype::ecs) is shared with the server.
namespace rtype::engine
{

/// The two drawing passes: the world first, then the interface on top of it. In VR, the world is drawn on the
/// board and the interface on a floating panel.
enum class Layer {
    World,
    Interface,
};

/// Makes an entity drawn, in a pass and at an order inside this pass: higher orders are drawn on top. An
/// entity is drawn with a Position and one of Text, Sprite, Box or Panel, which are drawn centred on the
/// position, or a Ring, drawn around it.
struct Drawable {
    Layer layer = Layer::World;
    int order = 0;
};

/// A text, centred on the position of the entity.
struct Text {
    std::string text;
    render::FontId font;
    float size = 48.0f;
    render::Color color = render::colors::White;
    /// Space added between two letters, in game units: widely spaced capitals.
    float spacing = 0.0f;
};

/// Writes the Text of the entity from a function, every frame: a score, a life count, a ping.
struct BoundText {
    std::function<std::string()> get;
};

/// Makes the Text of the entity a title: capitals in chrome with a light around them, drawn with
/// InterfaceStyle::title instead of the colour of the text. The size of the text is kept.
struct Title {};

/// A texture or a part of it (one frame of a sprite sheet), centred on the position of the entity.
struct Sprite {
    render::TextureId texture;
    /// Part of the texture, in texture pixels.
    render::Rectangle source;
    /// Size on screen, in game units.
    float width = 0.0f;
    float height = 0.0f;
    render::Color tint = render::colors::White;
    /// Clockwise rotation around its centre, in degrees.
    float rotation = 0.0f;
};

/// A rectangle, filled or outlined, centred on the position of the entity: backgrounds, frames, placeholders.
struct Box {
    float width = 0.0f;
    float height = 0.0f;
    render::Color color = render::colors::White;
    /// Width of the outline, drawn inside the box, or 0 for a filled box.
    float outline = 0.0f;
};

/// A circle lit along a part of its length, centred on the position of the entity: a countdown that runs
/// out. The light starts on the right of the circle and goes round anticlockwise, over the top, with a
/// spark at its end; the rest of the circle is a dark track. Without a Text, Sprite or Box, an Extent places
/// it with an Anchor.
struct Ring {
    /// Outer radius, in game units.
    float radius = 40.0f;
    float thickness = 3.0f;
    /// Part of the circle lit, from 0 to 1.
    float value = 1.0f;
    render::Color track{38, 41, 45, 255};
    render::Color light = render::colors::White;
};

/// What makes the text of a button a button: no frame, the text alone, crossed by a line of light with a star
/// after it while the button is lit, drawn with InterfaceStyle::flare. The button is as wide as its text.
struct Panel {
    float height = 0.0f;
    /// 0 for the normal look, 1 for the lit one, in between while the button fades from one to the other. It
    /// goes above 1 for a moment when the button is pressed, up to FlareStyle::pressedHighlight.
    float highlight = 0.0f;
    /// No line of light nor star: the text alone fades smoothly from its normal look to the lit one, with a
    /// halo, and is centred on the button. For the arrows of a carousel.
    bool plain = false;
};

/// A 3D model, seen through the Camera of the scenes. It is drawn in the World layer, before the 2D entities,
/// so it is behind them; it needs a Drawable but no Position. Transparent models (clouds, glass) must have a
/// higher Drawable order than what they cover.
struct Model {
    render::ModelId model;
    /// Where the origin of the model is, in world units.
    render::Vector3 position;
    /// Axis the model is turned around, and rotationAngle how much, in degrees.
    render::Vector3 rotationAxis{0.0f, 1.0f, 0.0f};
    float rotationAngle = 0.0f;
    float scale = 1.0f;
    render::Color tint = render::colors::White;
    /// How much the model is turned around its own vertical axis, in degrees, before rotationAxis: a ring
    /// keeps its tilt while it spins. Moved by a Spin.
    float spinAngle = 0.0f;
    /// Meshes of the model not drawn, by index in the model file: the flame of a ship that does not fly. Only
    /// for a model lit by the light.
    std::vector<int> hiddenMeshes;
    /// Set for a model of energy (a shot, a beam): it glows over what is behind it instead of being lit by
    /// the light (render::Renderer::drawEnergyModel()).
    std::optional<render::Energy> energy;
};

/// The camera that the 3D models are seen through. The first visible entity with a Camera is used; without
/// one, no model is drawn. Give it a SceneMember so that each scene has its own point of view.
struct Camera {
    render::Camera view;
};

/// The light of the 3D models. The first visible entity with a Light and without a Model lights the scene;
/// without one, models get the default render::Light. A Light on the entity of a Model lights only that
/// model, instead of the light of the scene: a station lit from the front while the planet keeps its night
/// side.
struct Light {
    render::Light light;
};

/// A picture that covers the whole screen, behind everything else of the World layer, 3D models included: the
/// sky of a scene. It is scaled to cover the visible area and cut on the sides that overflow, so it keeps its
/// proportions on any screen. The first shown entity with a Backdrop is drawn; it needs no Drawable nor
/// Position.
struct Backdrop {
    render::TextureId texture;
    render::Color tint = render::colors::White;
};

/// Size factor of what the entity draws, 1 by default. Animated by tweens, for instance to make a button
/// bounce.
struct Scale {
    float value = 1.0f;
};

/// Transparency of what the entity draws, from 0 (invisible) to 1 (opaque, the default).
struct Opacity {
    float value = 1.0f;
};

} // namespace rtype::engine
