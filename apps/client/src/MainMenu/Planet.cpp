#include "MainMenu/Planet.hpp"

#include "Assets.hpp"
#include "MainMenu/Space.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Scene/Scenes.hpp"

#include <cmath>

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

/// The view in the asteroids, in units of the models: in the gap between the inner ring, which ends at 18.8
/// from the centre, and the outer one, which starts at 19.9, a little above the rocks, which rise to 0.5.
constexpr float ringGapRadius = 19.3f;
constexpr float viewHeight = 0.8f;
/// The view looks along the rings, turned a little towards the planet so that the curve of the rings fills
/// it, and down towards their plane, in degrees.
constexpr float viewTurnTowardsPlanet = 12.0f;
constexpr float viewDip = 4.0f;

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

render::Camera createAsteroidsView(const render::Camera &view)
{
    const Rotation tilt{PlanetSystem.rotationAxis, PlanetSystem.rotationAngle};
    // The viewer is above the plane of the rings, on the side of their normal
    const render::Vector3 normal = rotate({0.0f, 1.0f, 0.0f}, tilt);
    const render::Vector3 towardsViewer = view.position - PlanetSystem.position;
    const render::Vector3 outwards = normalize(towardsViewer - (normal * dot(towardsViewer, normal)));
    // Along the rings, towards the left of the screen, where there is nothing behind them
    render::Vector3 along = cross(normal, outwards);
    if (along.x > 0.0f) {
        along = along * -1.0f;
    }
    const float turn = toRadians(viewTurnTowardsPlanet);
    const render::Vector3 forward = normalize((along * std::cos(turn)) - (outwards * std::sin(turn)) -
                                              (normal * std::sin(toRadians(viewDip))));

    render::Camera asteroids = view;
    asteroids.position =
        PlanetSystem.position + (((outwards * ringGapRadius) + (normal * viewHeight)) * PlanetSystem.scale);
    asteroids.target = asteroids.position + forward;
    asteroids.up = normal;
    return asteroids;
}

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
