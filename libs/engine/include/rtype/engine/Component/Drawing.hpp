#pragma once

#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

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
/// entity is drawn with a Position and one of Text, Sprite or Box, which are drawn centred on the position.
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
