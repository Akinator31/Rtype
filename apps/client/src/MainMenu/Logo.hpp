#pragma once

#include "rtype/ecs/Entity/Entity.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

namespace rtype::client
{

/// The logo is a picture of 1920 x 1080 pixels, drawn at logoWidth game units.
inline constexpr render::Rectangle logoPicture{0.0f, 0.0f, 1920.0f, 1080.0f};
inline constexpr float logoWidth = 1500.0f;

/// The logo of the game, in the middle of the screen. The loading screen and the main menu both draw it there,
/// so that it stays still when the main menu replaces the loading screen.
ecs::entity_t createLogo(engine::InterfaceBuilder &interface, render::ResourceManager &resources);

} // namespace rtype::client
