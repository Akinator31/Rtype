#include "MainMenu/Station.hpp"

#include "Assets.hpp"
#include "rtype/engine/Component/Drawing.hpp"

namespace rtype::client
{

namespace
{

/// Far behind the planet, on the right and a little above the centre of the screen. The model is about 264
/// units long: scaled down, it looks small and far.
constexpr render::Vector3 stationPosition{22.5f, 8.0f, -120.0f};
constexpr float stationScale = 0.11f;

/// Tilted towards the camera, so that its top can be seen.
constexpr render::Vector3 stationTiltAxis{1.0f, 0.0f, 0.0f};
constexpr float stationTilt = 20.0f;

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

} // namespace

void createStation(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources)
{
    const ecs::entity_t station = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(station, scene);
    ecs.addComponent<engine::Drawable>(station, engine::Layer::World, 0);
    ecs.addComponent<engine::Model>(station, resources.loadModel(RTYPE_MODEL_STATION), stationPosition,
                                    stationTiltAxis, stationTilt, stationScale);
    ecs.addComponent<engine::Light>(station, createStationLight());
}

} // namespace rtype::client
