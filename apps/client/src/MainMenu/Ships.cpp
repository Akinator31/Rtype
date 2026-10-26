#include "MainMenu/Ships.hpp"

#include "Assets.hpp"
#include "MainMenu/Flight.hpp"
#include "MainMenu/Space.hpp"
#include "MainMenu/Station.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"

#include <algorithm>
#include <array>
#include <random>
#include <vector>

namespace rtype::client
{

namespace
{

/// The ship models, made with the same rig: same animations, same orientation, belly at the same height.
/// There is one more model than ships: a ship that comes back is the one that no other ship flies.
constexpr std::array shipModelPaths{RTYPE_MODEL_SHIP_EZNO, RTYPE_MODEL_SHIP_SPACESHIP,
                                    RTYPE_MODEL_SHIP_BATTLESTAR, RTYPE_MODEL_SHIP_DROP_SHIP};

/// The ship models are 15 to 25 units wide: much smaller than the station. In the same proportion to the
/// station as its pads: 0.07 for a station scaled by 0.11.
constexpr float shipScale = 0.1086f;

/// The ship models point their nose to +x, their top to +y. A quarter turn around its vertical axis, before
/// its rotation, points the nose of a ship to +z, as the FlightSystem expects.
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

/// The ships wait there at first, under the screen, in the frame of the view of the menu: none is seen when
/// the menu opens, they arrive one after the other.
constexpr render::Vector3 waitingPoint{30.0f, -60.0f, -80.0f};

std::vector<ShipModel> loadShipModels(render::ResourceManager &resources)
{
    std::vector<ShipModel> models;
    for (const char *path : shipModelPaths) {
        models.push_back(loadShipModel(resources, path));
    }
    return models;
}

/// The models in a random order: each ship starts as one of them, not the same one at each launch.
std::vector<ShipModel> shuffleShipModels(std::vector<ShipModel> models)
{
    std::shuffle(models.begin(), models.end(), std::mt19937(std::random_device{}()));
    return models;
}

Flight createFlight(const FlightArea &area, const Dock &dock, const std::vector<ShipModel> &models,
                    const ShipModel &model)
{
    Flight flight;
    flight.area = area;
    flight.models = models;
    flight.animations = model.animations;
    flight.leavesTowardsCamera = dock.leavesTowardsCamera;
    flight.dock = convertStationPointToWorld(dock.point);
    flight.hover = convertStationPointToWorld({dock.point.x, dock.point.y + hoverHeight, dock.point.z});
    flight.heading = convertStationDirectionToWorld(stationLength);
    flight.up = convertStationDirectionToWorld(stationTop);
    flight.stage = FlightStage::Away;
    flight.duration = dock.delay;
    flight.from = convertViewPointToWorld(area.view, waitingPoint);
    flight.control = flight.from;
    flight.to = flight.from;
    return flight;
}

static_assert(docks.size() <= shipModelPaths.size(), "each ship starts with its own model");

} // namespace

void createShips(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources,
                 const FlightArea &area)
{
    const std::vector<ShipModel> models = loadShipModels(resources);
    const std::vector<ShipModel> firstModels = shuffleShipModels(models);
    for (std::size_t index = 0; index < docks.size(); ++index) {
        const ShipModel &model = firstModels[index];
        const Flight flight = createFlight(area, docks[index], models, model);
        const ecs::entity_t ship = ecs.createEntity();
        ecs.addComponent<engine::SceneMember>(ship, scene);
        ecs.addComponent<engine::Drawable>(ship, engine::Layer::World, 1);
        ecs.addComponent<engine::Model>(ship, model.model, flight.to, flight.up, 0.0f, shipScale,
                                        render::colors::White, noseToFront);
        ecs.addComponent<engine::ModelAnimation>(ship, model.animations.idle);
        ecs.addComponent<engine::Light>(ship, createStationLight());
        ecs.addComponent<Flight>(ship, flight);
    }
}

} // namespace rtype::client
