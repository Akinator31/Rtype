#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/render/ResourceManager.hpp"

namespace rtype::client
{

/// Creates the space station of the main menu: far in the background, on the right of the screen, seen a
/// little from above, with its own light so that the side facing the camera is lit.
void createStation(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources);

} // namespace rtype::client
