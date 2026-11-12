#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

namespace rtype::client
{

/// Creates the desert planet of the main menu, on the left of the screen, cut by its edge: the planet and its
/// dust, surrounded by two rings of asteroids and a ring of dust. Everything turns slowly around the axis of
/// the planet, with a SpinSystem.
void createPlanetSystem(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources);

/// Where the centre of the planet is, and its radius, in world units: the ships of the menu hide behind it.
[[nodiscard]] render::Vector3 getPlanetCentre();
[[nodiscard]] float getPlanetRadius();
/// The direction the rings of the planet face, of length 1: straight above their plane, on the side of the
/// camera of the menu.
[[nodiscard]] render::Vector3 getRingsNormal();

/// A view from inside the rings of asteroids, close to the rocks, on the side of the planet facing the menu:
/// it looks along the rings, so that only asteroids are seen, not the planet. A camera flying
/// from there to the view of the menu leaves the rings without crossing the planet.
/// @param view The camera of the menu: the view keeps its field of view.
[[nodiscard]] render::Camera createAsteroidsView(const render::Camera &view);

} // namespace rtype::client
