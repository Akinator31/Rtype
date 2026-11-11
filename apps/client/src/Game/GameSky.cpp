#include "Game/GameSky.hpp"

#include "Assets.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"

#include <array>
#include <cstddef>
#include <utility>

namespace rtype::client
{

namespace
{

/// A layer of the sky of the game, covering the screen, scrolling at its speed in game units per second.
struct SkyLayer {
    const char *path;
    float speed;
};

/// A picture passing in the sky of the game, between the layers, as in engine::BackdropSprite.
struct SkyObject {
    const char *path;
    float speed;
    float height;
    float y;
    float period;
    float start;
};

/// The layers from the back to the front: the nearer, the faster, so that the ships seem to fly to the right.
/// Even the deep sky moves visibly: nothing of the sky stays still on the screen.
const std::array<SkyLayer, 4> SkyLayers{{
    {RTYPE_IMAGE_SKY_DEEP, 40.0f},
    {RTYPE_IMAGE_SKY_NEBULA, 80.0f},
    {RTYPE_IMAGE_SKY_STARS, 140.0f},
    {RTYPE_IMAGE_SKY_NEAR, 300.0f},
}};

/// The objects of the sky, each drawn just in front of a layer: the galaxy in front of the deep sky, the
/// moon and the ringed planet in front of the stars, which they hide, at about their speed. They come back
/// every minute or so, the galaxy seen from the start.
const std::array<std::pair<std::size_t, SkyObject>, 3> SkyObjects{{
    {0, {RTYPE_IMAGE_SKY_GALAXY, 50.0f, 360.0f, 280.0f, 4200.0f, 1300.0f}},
    {2, {RTYPE_IMAGE_SKY_MOON, 160.0f, 200.0f, 760.0f, 9000.0f, 3000.0f}},
    {2, {RTYPE_IMAGE_SKY_PLANET, 170.0f, 640.0f, 380.0f, 13000.0f, 8000.0f}},
}};

/// The order of the Backdrop of a layer: room is left in between for the objects in front of it.
int skyOrder(std::size_t layer)
{
    return static_cast<int>(layer) * 2;
}

} // namespace

void createGameSky(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources, float time)
{
    for (std::size_t layer = 0; layer < SkyLayers.size(); ++layer) {
        const ecs::entity_t entity = ecs.createEntity();
        ecs.addComponent<engine::SceneMember>(entity, scene);
        ecs.addComponent<engine::Backdrop>(
            entity, resources.loadTexture(SkyLayers[layer].path, render::TextureFiltering::Smooth),
            render::colors::White, skyOrder(layer));
        const float speed = SkyLayers[layer].speed;
        ecs.addComponent<engine::Scroll>(entity, speed, speed * time);
    }
    for (const auto &[layer, object] : SkyObjects) {
        const ecs::entity_t entity = ecs.createEntity();
        ecs.addComponent<engine::SceneMember>(entity, scene);
        ecs.addComponent<engine::BackdropSprite>(
            entity, engine::BackdropSprite{
                        .texture = resources.loadTexture(object.path, render::TextureFiltering::Smooth),
                        .height = object.height,
                        .y = object.y,
                        .period = object.period,
                        .start = object.start,
                        .order = skyOrder(layer) + 1});
        ecs.addComponent<engine::Scroll>(entity, object.speed, object.speed * time);
    }
}

} // namespace rtype::client
