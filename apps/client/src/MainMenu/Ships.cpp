#include "MainMenu/Ships.hpp"

#include "Assets.hpp"
#include "MainMenu/Flight.hpp"
#include "MainMenu/Space.hpp"
#include "MainMenu/Station.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"

#include <array>

namespace rtype::client
{

namespace
{

/// The ship model is about 24 units wide: much smaller than the station.
constexpr float shipScale = 0.07f;

/// The ship model points its nose to +x, its top to +y. A quarter turn around its vertical axis, before its
/// rotation, points its nose to +z, as the FlightSystem expects.
constexpr float noseToFront = -90.0f;

/// Where a ship lands, in units of the station model, how long it waits out of the screen before its first
/// approach, in seconds, and whether it leaves towards the camera.
struct Dock {
    render::Vector3 point;
    float delay;
    bool leavesTowardsCamera;
};

/// Three of the four hexagonal landing pads of the station, at the ends of its arms: a few ships, so that the
/// menu stays calm. The points are above the centres of the pads by the depth of the belly of a ship under
/// its origin, for the ships to rest on them. The first ship leaves towards the camera.
constexpr std::array docks{
    Dock{{-42.65f, 3.3f, 50.05f}, 1.0f, true},
    Dock{{2.85f, 3.3f, 50.05f}, 9.0f, false},
    Dock{{2.85f, 3.3f, -50.05f}, 18.0f, false},
};

/// The ships go down onto their docks from this height above them, in units of the station model: high enough
/// to fly over a ship resting on a pad.
constexpr float hoverHeight = 18.0f;

/// The ships fly along the length of the station, towards its far end.
constexpr render::Vector3 stationLength{1.0f, 0.0f, 0.0f};
constexpr render::Vector3 stationTop{0.0f, 1.0f, 0.0f};

/// The ships wait there at first, under the screen: none is seen when the menu opens, they arrive one after
/// the other.
constexpr render::Vector3 waitingPoint{30.0f, -40.0f, -60.0f};

ShipAnimations findShipAnimations(const render::ResourceManager &resources, render::ModelId model)
{
    return {resources.getModelAnimation(model, "Idle"), resources.getModelAnimation(model, "Move"),
            resources.getModelAnimation(model, "MoveUp"), resources.getModelAnimation(model, "MoveDown")};
}

Flight createFlight(const Dock &dock, const ShipAnimations &animations)
{
    Flight flight;
    flight.animations = animations;
    flight.leavesTowardsCamera = dock.leavesTowardsCamera;
    flight.dock = convertStationPointToWorld(dock.point);
    flight.hover = convertStationPointToWorld({dock.point.x, dock.point.y + hoverHeight, dock.point.z});
    flight.heading = convertStationDirectionToWorld(stationLength);
    flight.up = convertStationDirectionToWorld(stationTop);
    flight.stage = FlightStage::Away;
    flight.duration = dock.delay;
    flight.from = waitingPoint;
    flight.control = waitingPoint;
    flight.to = waitingPoint;
    return flight;
}

} // namespace

void createShips(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources)
{
    const render::ModelId model = resources.loadModel(RTYPE_MODEL_SHIP);
    const ShipAnimations animations = findShipAnimations(resources, model);
    for (const Dock &dock : docks) {
        const Flight flight = createFlight(dock, animations);
        const ecs::entity_t ship = ecs.createEntity();
        ecs.addComponent<engine::SceneMember>(ship, scene);
        ecs.addComponent<engine::Drawable>(ship, engine::Layer::World, 1);
        ecs.addComponent<engine::Model>(ship, model, flight.to, flight.up, 0.0f, shipScale,
                                        render::colors::White, noseToFront);
        ecs.addComponent<engine::ModelAnimation>(ship, animations.idle);
        ecs.addComponent<engine::Light>(ship, createStationLight());
        ecs.addComponent<Flight>(ship, flight);
    }
}

} // namespace rtype::client
