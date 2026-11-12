#pragma once

#include "Looks.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Scene/Scenes.hpp"

namespace orbit
{

/// The walls on the left, the right and the top of the arena, with a line of light along their inner side:
/// the bottom is open, the ball is lost there.
void spawnWalls(rtype::ecs::Ecs &ecs, rtype::engine::SceneId scene);
/// The paddle, in the middle of the bottom of the arena, over a soft light.
rtype::ecs::entity_t spawnPaddle(rtype::ecs::Ecs &ecs, rtype::engine::SceneId scene, const Looks &looks);
/// A ball waiting on the paddle to be launched, in a halo.
void spawnBall(rtype::ecs::Ecs &ecs, rtype::engine::SceneId scene, const Looks &looks);
/// The bricks of a level (see levelLayout()), lit from the top.
void spawnBricks(rtype::ecs::Ecs &ecs, rtype::engine::SceneId scene, int level);
/// The sky of a scene: layers of space sliding at different speeds, and a planet passing slowly.
void spawnSky(rtype::ecs::Ecs &ecs, rtype::engine::SceneId scene, const Looks &looks);

} // namespace orbit
