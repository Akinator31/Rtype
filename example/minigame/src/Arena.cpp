#include "Arena.hpp"

#include "Breaker.hpp"
#include "Levels.hpp"
#include "Visuals.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/physics/Physics.hpp"

#include <array>
#include <cstddef>

namespace orbit
{

namespace
{

namespace ecs = rtype::ecs;
namespace engine = rtype::engine;
namespace physics = rtype::physics;
using rtype::render::Color;

constexpr Color wallColor{16, 20, 42, 215};
constexpr Color edgeColor{90, 210, 255, 150};
constexpr float edgeThickness = 3.0f;
constexpr Color ballTop{255, 255, 255, 255};
constexpr Color ballBottom{150, 225, 255, 255};
constexpr Color glowTint{90, 200, 255, 140};
/// The lights under the ball and the paddle, and the lines of the walls over them.
constexpr int glowOrder = -1;
constexpr int edgeOrder = 1;
/// The speeds of the layers of the sky, the farthest the slowest, in game units per second.
constexpr std::array<float, 4> skySpeeds{4.0f, 10.0f, 22.0f, 40.0f};

/// An entity of the scene drawn as a box, with a collider of the same size on its layer.
ecs::entity_t spawnBox(ecs::Ecs &ecs, engine::SceneId scene, float x, float y, float width, float height,
                       engine::Box look, std::uint32_t layer, std::uint32_t mask)
{
    const ecs::entity_t entity = spawnDrawn(ecs, scene, x, y);
    look.width = width;
    look.height = height;
    ecs.addComponent<engine::Box>(entity, look);
    ecs.addComponent<physics::Collider>(
        entity, physics::Collider{physics::Aabb{{}, {width / 2.0f, height / 2.0f, 0.0f}}, {}, false});
    ecs.addComponent<physics::CollisionLayer>(entity, layer);
    ecs.addComponent<physics::CollisionMask>(entity, mask);
    return entity;
}

/// A soft light under an entity, which follows it and goes with it.
void addGlow(ecs::Ecs &ecs, engine::SceneId scene, ecs::entity_t owner, const Picture &glow, float width,
             float height)
{
    const ecs::entity_t light = spawnDrawn(ecs, scene, 0.0f, 0.0f, engine::Layer::World, glowOrder);
    ecs.addComponent<engine::Sprite>(light, glow.texture, glow.source, width, height, glowTint);
    ecs::attach(ecs, light, owner);
}

void spawnWall(ecs::Ecs &ecs, engine::SceneId scene, float x, float y, float width, float height,
               std::uint32_t mask)
{
    const ecs::entity_t wall =
        spawnBox(ecs, scene, x, y, width, height, {.color = wallColor}, WallLayer, mask);
    ecs.addComponent<physics::Static>(wall);
}

void spawnEdge(ecs::Ecs &ecs, engine::SceneId scene, float x, float y, float width, float height)
{
    ecs.addComponent<engine::Box>(spawnDrawn(ecs, scene, x, y, engine::Layer::World, edgeOrder), width,
                                  height, edgeColor);
}

} // namespace

void spawnWalls(ecs::Ecs &ecs, engine::SceneId scene)
{
    const float half = WallThickness / 2.0f;
    const float height = ArenaHeight - FieldTop;
    const float middle = FieldTop + (height / 2.0f);
    spawnWall(ecs, scene, half, middle, WallThickness, height, BallLayer | PaddleLayer);
    spawnWall(ecs, scene, ArenaWidth - half, middle, WallThickness, height, BallLayer | PaddleLayer);
    spawnWall(ecs, scene, ArenaWidth / 2.0f, FieldTop + half, ArenaWidth, WallThickness, BallLayer);
    spawnEdge(ecs, scene, WallThickness, middle, edgeThickness, height);
    spawnEdge(ecs, scene, ArenaWidth - WallThickness, middle, edgeThickness, height);
    spawnEdge(ecs, scene, ArenaWidth / 2.0f, FieldTop + WallThickness, ArenaWidth - (2.0f * WallThickness),
              edgeThickness);
}

ecs::entity_t spawnPaddle(ecs::Ecs &ecs, engine::SceneId scene, const Looks &looks)
{
    const ecs::entity_t paddle =
        spawnBox(ecs, scene, ArenaWidth / 2.0f, PaddleY, PaddleWidth, PaddleHeight,
                 {.color = PaddleTop, .bottom = PaddleBottom}, PaddleLayer, WallLayer | BallLayer);
    ecs.addComponent<physics::Velocity>(paddle);
    ecs.addComponent<Paddle>(paddle);
    addGlow(ecs, scene, paddle, looks.glow, PaddleWidth * 1.6f, PaddleHeight * 5.0f);
    return paddle;
}

void spawnBall(ecs::Ecs &ecs, engine::SceneId scene, const Looks &looks)
{
    const ecs::entity_t ball =
        spawnBox(ecs, scene, ArenaWidth / 2.0f, PaddleY - BallSize, BallSize, BallSize,
                 {.color = ballTop, .bottom = ballBottom}, BallLayer, WallLayer | BrickLayer | PaddleLayer);
    // The ball bounces by itself (BallSystem): the physics only reports what it touches, and sweeps it so
    // that it does not go through a brick between two frames
    ecs.getComponent<physics::Collider>(ball).trigger = true;
    ecs.addComponent<physics::FastMoving>(ball);
    ecs.addComponent<physics::Velocity>(ball);
    ecs.addComponent<Ball>(ball);
    addGlow(ecs, scene, ball, looks.glow, BallSize * 5.0f, BallSize * 5.0f);
}

void spawnBricks(ecs::Ecs &ecs, engine::SceneId scene, int level)
{
    for (const BrickSpec &spec : levelLayout(level)) {
        const ecs::entity_t brick =
            spawnBox(ecs, scene, spec.x, spec.y, BrickWidth, BrickHeight,
                     {.color = brickColor(spec.strength), .bottom = brickBottom(spec.strength)}, BrickLayer,
                     BallLayer);
        ecs.addComponent<physics::Static>(brick);
        ecs.addComponent<Brick>(brick, spec.strength);
    }
}

void spawnSky(ecs::Ecs &ecs, engine::SceneId scene, const Looks &looks)
{
    for (std::size_t layer = 0; layer < looks.sky.size(); ++layer) {
        const ecs::entity_t sky = ecs.createEntity();
        ecs.addComponent<engine::SceneMember>(sky, scene);
        ecs.addComponent<engine::Backdrop>(sky, looks.sky[layer], rtype::render::colors::White,
                                           static_cast<int>(layer * 2));
        ecs.addComponent<engine::Scroll>(sky, skySpeeds[layer]);
    }
    const ecs::entity_t planet = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(planet, scene);
    ecs.addComponent<engine::BackdropSprite>(planet, looks.planet.texture, 520.0f, 330.0f, 5200.0f, 1400.0f,
                                             Color{200, 200, 220, 255}, 3);
    ecs.addComponent<engine::Scroll>(planet, 12.0f);
}

} // namespace orbit
