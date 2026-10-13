#pragma once

#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <functional>
#include <string>

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
/// position.
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
};

/// Writes the Text of the entity from a function, every frame: a score, a life count, a ping.
struct BoundText {
    std::function<std::string()> get;
};

/// A texture or a part of it (one frame of a sprite sheet), centred on the position of the entity.
struct Sprite {
    render::TextureId texture;
    /// Part of the texture, in texture pixels.
    render::Rectangle source;
    /// Size on screen, in game units.
    float width = 0.0f;
    float height = 0.0f;
    render::Color tint = render::colors::White;
};

/// A rectangle, filled or outlined, centred on the position of the entity: backgrounds, frames, placeholders.
struct Box {
    float width = 0.0f;
    float height = 0.0f;
    render::Color color = render::colors::White;
    /// Width of the outline, drawn inside the box, or 0 for a filled box.
    float outline = 0.0f;
};

/// A rounded glass panel, centred on the position of the entity, behind its text: the frame of a button. It
/// is drawn with the ButtonStyle of the interface, between its normal and its focused look.
struct Panel {
    float width = 0.0f;
    float height = 0.0f;
    /// 0 for the normal look, 1 for the focused one, in between while the button fades from one to the other.
    float highlight = 0.0f;
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
};

/// The camera that the 3D models are seen through. The first visible entity with a Camera is used; without
/// one, no model is drawn. Give it a SceneMember so that each scene has its own point of view.
struct Camera {
    render::Camera view;
};

/// The light of the 3D models. The first visible entity with a Light is used; without one, models get the
/// default render::Light.
struct Light {
    render::Light light;
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
