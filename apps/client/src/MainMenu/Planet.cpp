#include "MainMenu/Planet.hpp"

#include "Assets.hpp"

#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Scene/Scenes.hpp"

namespace rtype::client
{

namespace
{

/// Where a model is placed in the world.
struct Placement {
    render::Vector3 position;
    render::Vector3 rotationAxis;
    float rotationAngle = 0.0f;
    float scale = 1.0f;
};

/// A far planet, cut by the left edge of the screen only.
const Placement DesertPlanet{{-25.0f, 0.0f, -60.0f}, {0.2f, 1.0f, 0.1f}, 120.0f, 1.3f};

/// Creates an entity drawing a 3D model of the scene, at a drawing order.
void createModel(ecs::Ecs &ecs, engine::SceneId scene, render::ModelId model, const Placement &placement,
                 int order)
{
    const ecs::entity_t entity = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(entity, scene);
    ecs.addComponent<engine::Drawable>(entity, engine::Layer::World, order);
    ecs.addComponent<engine::Model>(entity, model, placement.position, placement.rotationAxis,
                                    placement.rotationAngle, placement.scale);
}

} // namespace

/// The clouds are transparent: drawn after the planet they cover.
void createDesertPlanet(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources)
{
    const render::ModelId planet = resources.loadModel(RTYPE_MODEL_DESERT_PLANET);
    const render::ModelId clouds = resources.loadModel(RTYPE_MODEL_DESERT_CLOUDS);
    createModel(ecs, scene, planet, DesertPlanet, 0);
    createModel(ecs, scene, clouds, DesertPlanet, 1);
}

} // namespace rtype::client
