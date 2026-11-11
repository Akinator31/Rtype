#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/render/ResourceManager.hpp"

namespace rtype::client
{

/// Creates the scrolling sky of the game in a scene: its layers and the objects passing between them, the
/// nearer, the faster. The game scene, its results, and the hangar at the end of the take-off show it.
/// @param time How long the sky has scrolled already, in seconds: a game opened from the hangar goes on with
/// the sky the hangar showed.
void createGameSky(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources,
                   float time = 0.0f);

} // namespace rtype::client
