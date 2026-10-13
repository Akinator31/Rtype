#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/render/ResourceManager.hpp"

namespace rtype::client
{

/// Creates the desert planet of the main menu and its clouds, on the left of the screen, cut by its edge.
void createDesertPlanet(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources);

} // namespace rtype::client
