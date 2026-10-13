#include "MainMenu/Planet.hpp"

#include "Assets.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Component/Animation.hpp"
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

/// A far planet, cut by the left edge of the screen only. The models of the planet system share their centre:
/// they all get this placement.
const Placement PlanetSystem{{-25.0f, 0.0f, -60.0f}, {0.2f, 1.0f, 0.1f}, 120.0f, 1.3f};

/// Speeds of the rotations, in degrees per second: the planet turns very slowly on itself, its dust a little
/// faster, as if blown by the wind; the inner ring goes faster than the outer one, like closer orbits.
constexpr float planetSpeed = 0.8f;
constexpr float planetDustSpeed = 1.1f;
constexpr float innerRingSpeed = 2.0f;
constexpr float outerRingSpeed = 1.2f;

/// Drawing orders: the opaque models first, then the transparent dust over what it covers.
enum DrawingOrder : int {
    PlanetOrder,
    InnerRingOrder,
    OuterRingOrder,
    PlanetDustOrder,
    RingDustOrder,
};

/// Creates an entity drawing a 3D model of the planet system, at a drawing order.
ecs::entity_t createModel(ecs::Ecs &ecs, engine::SceneId scene, render::ModelId model, int order)
{
    const ecs::entity_t entity = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(entity, scene);
    ecs.addComponent<engine::Drawable>(entity, engine::Layer::World, order);
    ecs.addComponent<engine::Model>(entity, model, PlanetSystem.position, PlanetSystem.rotationAxis,
                                    PlanetSystem.rotationAngle, PlanetSystem.scale);
    return entity;
}

/// Creates a model of the planet system turning around the axis of the planet: the planet on itself, its
/// dust, the rings around it.
void createSpinningModel(ecs::Ecs &ecs, engine::SceneId scene, render::ModelId model, int order, float speed)
{
    const ecs::entity_t entity = createModel(ecs, scene, model, order);
    ecs.addComponent<engine::Spin>(entity, speed);
}

} // namespace

void createPlanetSystem(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources)
{
    createSpinningModel(ecs, scene, resources.loadModel(RTYPE_MODEL_PLANET), PlanetOrder, planetSpeed);
    createSpinningModel(ecs, scene, resources.loadModel(RTYPE_MODEL_RING_ASTEROIDS_INNER), InnerRingOrder,
                        innerRingSpeed);
    createSpinningModel(ecs, scene, resources.loadModel(RTYPE_MODEL_RING_ASTEROIDS_OUTER), OuterRingOrder,
                        outerRingSpeed);
    createSpinningModel(ecs, scene, resources.loadModel(RTYPE_MODEL_PLANET_DUST), PlanetDustOrder,
                        planetDustSpeed);
    createSpinningModel(ecs, scene, resources.loadModel(RTYPE_MODEL_RING_DUST), RingDustOrder,
                        outerRingSpeed);
}

} // namespace rtype::client
