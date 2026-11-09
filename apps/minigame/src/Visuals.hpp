#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"
#include "rtype/engine/Scene/Scenes.hpp"

#include <string>

/// The building blocks of the look of the game, shared by the arena, the effects, the bar and the menus.
namespace orbit
{

/// The paddle, and the small paddles that count the lives: light at the top, deep blue at the bottom.
inline constexpr rtype::render::Color PaddleTop{170, 245, 255, 255};
inline constexpr rtype::render::Color PaddleBottom{30, 120, 200, 255};

/// An entity of the scene, at this place, drawn in this layer at this order: give it a Box, a Sprite or a
/// Text.
rtype::ecs::entity_t spawnDrawn(rtype::ecs::Ecs &ecs, rtype::engine::SceneId scene, float x, float y,
                                rtype::engine::Layer layer = rtype::engine::Layer::World, int order = 0);

/// Makes what the entity draws pop: from a size above or under its own, it bounces back to it.
void popIn(rtype::ecs::Ecs &ecs, rtype::ecs::entity_t entity, float from = 1.15f, float duration = 0.18f);

/// A value going back and forth forever: a shimmer, a blink, a breath.
[[nodiscard]] rtype::engine::Tween pingPong(rtype::engine::TweenTarget target, float from, float to,
                                            float duration);

/// A text of the interface at its own size and colour, instead of those of the style.
rtype::ecs::entity_t styledText(rtype::engine::InterfaceBuilder &interface, rtype::ecs::Ecs &ecs,
                                const std::string &text, rtype::engine::Anchor anchor, float size,
                                rtype::render::Color color);

} // namespace orbit
