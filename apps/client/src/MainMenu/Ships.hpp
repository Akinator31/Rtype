#pragma once

#include "MainMenu/Flight.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/render/ResourceManager.hpp"

namespace rtype::client
{

/// Creates the ships of the main menu: each one has its own dock on the station, lands on it, stays a while,
/// leaves out of the screen and comes back, flown by a FlightSystem. None is seen when the menu opens: they
/// arrive one after the other, then some land while others leave.
/// @param area The view of the menu and the planet, that the ships fly around.
void createShips(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources,
                 const FlightArea &area);

} // namespace rtype::client
