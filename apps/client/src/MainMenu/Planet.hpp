#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/render/ResourceManager.hpp"

namespace rtype::client
{

/// Creates the desert planet of the main menu, on the left of the screen, cut by its edge: the planet and its
/// dust, surrounded by two rings of asteroids and a ring of dust. Everything turns slowly around the axis of
/// the planet, with a SpinSystem.
void createPlanetSystem(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources);

} // namespace rtype::client
