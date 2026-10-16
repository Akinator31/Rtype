#include "MainMenu/Station.hpp"

#include "Assets.hpp"
#include "MainMenu/Space.hpp"
#include "rtype/engine/Component/Drawing.hpp"

namespace rtype::client
{

namespace
{

/// Far behind the planet, on the right of the screen, at the height of its centre. The model is about 264
/// units long: scaled down, it looks small and far.
constexpr render::Vector3 stationPosition{22.5f, -.8959122f, -74.042f};
constexpr float stationScale = 0.11f;

/// Tilted towards the camera, so that its top can be seen.
constexpr Rotation stationTilt{{0.6488686f, 0.5675164f, 0.50684774f}, 53.071587f};

} // namespace

/// The sun of the menu comes from behind the station, which would face the camera with its dark side: its own
/// light comes from the front, above and to the left, so that the side seen is lit. It is dimmer than the
/// sun, so that the far station does not stand out more than the planet.
render::Light createStationLight()
{
    render::Light light;
    light.direction = {0.5f, -0.4f, -1.0f};
    light.color = {160, 154, 142, 255};
    light.ambient = {20, 21, 28, 255};
    return light;
}

render::Vector3 convertStationPointToWorld(render::Vector3 point)
{
    return stationPosition + (rotate(point, stationTilt) * stationScale);
}

render::Vector3 convertStationDirectionToWorld(render::Vector3 direction)
{
    return rotate(direction, stationTilt);
}

void createStation(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources)
{
    const ecs::entity_t station = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(station, scene);
    ecs.addComponent<engine::Drawable>(station, engine::Layer::World, 0);
    ecs.addComponent<engine::Model>(station, resources.loadModel(RTYPE_MODEL_STATION), stationPosition,
                                    stationTilt.axis, stationTilt.angle, stationScale);
    ecs.addComponent<engine::Light>(station, createStationLight());
}

} // namespace rtype::client
