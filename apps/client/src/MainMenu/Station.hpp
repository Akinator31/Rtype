#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

namespace rtype::client
{

/// Creates the space station of the main menu: far in the background, on the right of the screen, seen a
/// little from above, with its own light so that the side facing the camera is lit.
void createStation(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources);

/// Converts a point of the station model, in model units, to where it is drawn in the world.
[[nodiscard]] render::Vector3 convertStationPointToWorld(render::Vector3 point);

/// Turns a direction of the station model as the station is turned in the world.
[[nodiscard]] render::Vector3 convertStationDirectionToWorld(render::Vector3 direction);

/// The light of the station, for the models next to it to be lit like it.
[[nodiscard]] render::Light createStationLight();

} // namespace rtype::client
