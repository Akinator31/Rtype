#include "MainMenu/FlightSystem.hpp"

#include "MainMenu/Space.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace rtype::client
{

namespace
{

/// Average speeds, in world units per second: the ships come in slowly, go down and up more slowly still,
/// and leave a little faster. They follow the size of the station (scaled by 0.17): the ships are as big
/// and as quick next to it whatever its scale.
constexpr float approachSpeed = 2.8f;
constexpr float verticalSpeed = 0.78f;
constexpr float departureSpeed = 3.4f;
/// The ship that leaves towards the camera has a long way to go: it flies faster, for its trip not to last
/// too long, and rushes past the camera at the end.
constexpr float returnSpeed = 6.2f;

/// How far from above the dock a ship flies straight along its heading, when it arrives and when it leaves.
constexpr float runwayLength = 12.4f;

/// How long a ship stays on its dock, and out of the screen, in seconds.
constexpr float shortestStay = 4.0f;
constexpr float longestStay = 10.0f;
constexpr float shortestAbsence = 2.0f;
constexpr float longestAbsence = 8.0f;

/// The points out of the screen are picked in the frame of the view of the menu (convertViewPointToWorld),
/// whose vertical field of view is 30 degrees: at a depth d, the screen goes up and down to
/// d * screenHalfHeight, and left and right to d * screenHalfWidth on the widest screens. A point further
/// than that by outOfScreenMargin, the size of a ship, is not seen.
constexpr float screenHalfHeight = 0.268f;
constexpr float screenHalfWidth = 0.48f;
constexpr float outOfScreenMargin = 6.0f;

/// The ships come from closer to the camera than the station, which is 132 units deep, and leave far behind
/// it.
constexpr float closestArrival = 80.0f;
constexpr float furthestArrival = 125.0f;
constexpr float closestExit = 145.0f;
constexpr float furthestExit = 230.0f;

/// Where the ships come from: behind the camera, behind the planet, or else from under the screen. From
/// behind the camera, they start just under it, out of the field of view, rise into it big on the screen and
/// fly away towards the station: they start in this box, in the frame of the view, where the ships that leave
/// towards the camera go back.
constexpr float fromCameraChance = 0.4f;
constexpr float fromPlanetChance = 0.35f;
constexpr render::Vector3 behindCameraLow{-1.8f, -0.6f, 0.5f};
constexpr render::Vector3 behindCameraHigh{-1.0f, -0.5f, 1.0f};

/// The ships that do not leave towards the camera go behind the planet once in two, else out of the screen.
constexpr float towardsPlanetChance = 0.5f;
/// Behind the planet, the ships are on a line from the camera through the planet, which hides them: it passes
/// at most this far from the centre of the planet, in radii, on the side of the station, and they are this
/// far behind the planet, in radii, so that they come out from behind its edge.
constexpr float planetLineSpread = 0.6f;
constexpr float behindPlanetDistance = 3.0f;

/// How long a ship takes to go from the pose of a stage to the next one, in seconds: they lean forward or
/// backward when they go down or up.
constexpr float animationFade = 1.0f;

/// The longest time a frame moves the ships by, in seconds. The frame after the menu opens lasts as long as
/// its models took to load, several seconds: without this limit, the ships would already be on the screen.
constexpr float longestStep = 0.1f;

/// The time below which a stage is considered over, so that a stage of no length cannot stop the system.
constexpr float shortestStage = 0.1f;

/// Slows down gently until it stops: a long approach keeps moving until it reaches the dock.
[[nodiscard]] float easeOut(float progress)
{
    const float remaining = 1.0f - progress;
    return 1.0f - (remaining * remaining);
}

/// Slows down more at the end, for a soft touch down.
[[nodiscard]] float easeOutStrongly(float progress)
{
    const float remaining = 1.0f - progress;
    return 1.0f - (remaining * remaining * remaining);
}

[[nodiscard]] float easeIn(float progress)
{
    return progress * progress;
}

/// Where the ship is on its route, from 0 at its start to 1 at its end.
[[nodiscard]] float computeRouteProgress(const Flight &flight)
{
    const float progress = std::min(flight.time / flight.duration, 1.0f);
    switch (flight.stage) {
    case FlightStage::Approach:
        return easeOut(progress);
    case FlightStage::Landing:
        return easeOutStrongly(progress);
    case FlightStage::TakeOff:
    case FlightStage::Departure:
        return easeIn(progress);
    case FlightStage::Docked:
    case FlightStage::Away:
        break;
    }
    return 1.0f;
}

[[nodiscard]] render::Vector3 computeRoutePoint(const Flight &flight, float progress)
{
    const float remaining = 1.0f - progress;
    return (flight.from * (remaining * remaining)) + (flight.control * (2.0f * progress * remaining)) +
           (flight.to * (progress * progress));
}

[[nodiscard]] render::Vector3 computeRouteDirection(const Flight &flight, float progress)
{
    return ((flight.control - flight.from) * (1.0f - progress)) + ((flight.to - flight.control) * progress);
}

[[nodiscard]] float computeRouteLength(render::Vector3 from, render::Vector3 control, render::Vector3 to)
{
    return length(control - from) + length(to - control);
}

/// Where the nose of the ship points: along its route while it flies, along its heading over its dock.
[[nodiscard]] render::Vector3 computeNoseDirection(const Flight &flight, float progress)
{
    const bool isFlying = flight.stage == FlightStage::Approach || flight.stage == FlightStage::Departure ||
                          flight.stage == FlightStage::Away;
    const render::Vector3 direction = normalize(computeRouteDirection(flight, progress));
    // Its top stays towards the top of the station: it does not fly straight up or down
    if (!isFlying || length(cross(flight.up, direction)) < 0.1f) {
        return flight.heading;
    }
    return direction;
}

void placeShip(engine::Model &model, const Flight &flight)
{
    const float progress = computeRouteProgress(flight);
    const Rotation rotation = computeRotationTowards(computeNoseDirection(flight, progress), flight.up);
    model.position = computeRoutePoint(flight, progress);
    model.rotationAxis = rotation.axis;
    model.rotationAngle = rotation.angle;
}

[[nodiscard]] int selectAnimation(const Flight &flight)
{
    switch (flight.stage) {
    case FlightStage::Approach:
    case FlightStage::Departure:
        return flight.animations.move;
    case FlightStage::Landing:
        return flight.animations.moveDown;
    case FlightStage::TakeOff:
        return flight.animations.moveUp;
    case FlightStage::Docked:
    case FlightStage::Away:
        break;
    }
    return flight.animations.idle;
}

void setRoute(Flight &flight, FlightStage stage, render::Vector3 from, render::Vector3 control,
              render::Vector3 to, float duration)
{
    flight.stage = stage;
    flight.duration = std::max(duration, shortestStage);
    flight.from = from;
    flight.control = control;
    flight.to = to;
}

void setStraightRoute(Flight &flight, FlightStage stage, render::Vector3 from, render::Vector3 to)
{
    setRoute(flight, stage, from, (from + to) * 0.5f, to, length(to - from) / verticalSpeed);
}

} // namespace

FlightSystem::FlightSystem(ecs::ComponentCaches &caches)
    : _flights(use<Flight>(caches)), _models(use<engine::Model>(caches)),
      _animations(use<engine::ModelAnimation>(caches)), _random(std::random_device{}())
{
}

float FlightSystem::pickBetween(float low, float high)
{
    return std::uniform_real_distribution<float>(low, high)(_random);
}

/// Under the bottom edge of the screen, or out of its right edge in its lower half.
render::Vector3 FlightSystem::pickPointBelowScreen(const FlightArea &area)
{
    const float depth = pickBetween(closestArrival, furthestArrival);
    if (pickBetween(0.0f, 1.0f) < 0.5f) {
        return convertViewPointToWorld(area.view, {depth * pickBetween(0.05f, screenHalfWidth),
                                                   -(depth * screenHalfHeight) - outOfScreenMargin, -depth});
    }
    return convertViewPointToWorld(area.view, {(depth * screenHalfWidth) + outOfScreenMargin,
                                               depth * pickBetween(-screenHalfHeight, 0.0f), -depth});
}

/// Over the top edge of the screen, or out of its right edge in its upper half.
render::Vector3 FlightSystem::pickPointAboveScreen(const FlightArea &area)
{
    const float depth = pickBetween(closestExit, furthestExit);
    if (pickBetween(0.0f, 1.0f) < 0.5f) {
        return convertViewPointToWorld(area.view, {depth * pickBetween(0.1f, screenHalfWidth),
                                                   (depth * screenHalfHeight) + outOfScreenMargin, -depth});
    }
    return convertViewPointToWorld(area.view, {(depth * screenHalfWidth) + outOfScreenMargin,
                                               depth * pickBetween(0.0f, screenHalfHeight), -depth});
}

render::Vector3 FlightSystem::pickPointBehindCamera(const FlightArea &area)
{
    return convertViewPointToWorld(area.view, {pickBetween(behindCameraLow.x, behindCameraHigh.x),
                                               pickBetween(behindCameraLow.y, behindCameraHigh.y),
                                               pickBetween(behindCameraLow.z, behindCameraHigh.z)});
}

/// Behind the planet as seen from the camera, on the side of the station: the ship comes out from behind the
/// edge of the planet that faces it.
render::Vector3 FlightSystem::pickPointBehindPlanet(const FlightArea &area, render::Vector3 station)
{
    const render::Vector3 eye = area.view.position;
    const render::Vector3 towardsPlanet = normalize(area.planetCentre - eye);
    const render::Vector3 towardsStation = station - area.planetCentre;
    const render::Vector3 side =
        normalize(towardsStation - (towardsPlanet * dot(towardsStation, towardsPlanet)));
    const render::Vector3 across = normalize(cross(towardsPlanet, side));
    const float sideOffset = pickBetween(0.0f, planetLineSpread);
    const float acrossOffset = pickBetween(-planetLineSpread, planetLineSpread) * 0.5f;
    const render::Vector3 hidden = area.planetCentre + (side * (area.planetRadius * sideOffset)) +
                                   (across * (area.planetRadius * acrossOffset));
    const float distance = length(area.planetCentre - eye) + (area.planetRadius * behindPlanetDistance);
    return eye + (normalize(hidden - eye) * distance);
}

void FlightSystem::startApproach(Flight &flight)
{
    const float origin = pickBetween(0.0f, 1.0f);
    render::Vector3 from = pickPointBelowScreen(flight.area);
    if (origin < fromCameraChance) {
        from = pickPointBehindCamera(flight.area);
    } else if (origin < fromCameraChance + fromPlanetChance) {
        from = pickPointBehindPlanet(flight.area, flight.hover);
    }
    const render::Vector3 control = flight.hover - (flight.heading * runwayLength);
    setRoute(flight, FlightStage::Approach, from, control, flight.hover,
             computeRouteLength(from, control, flight.hover) / approachSpeed);
}

void FlightSystem::startDeparture(Flight &flight)
{
    const render::Vector3 control = flight.hover + (flight.heading * runwayLength);
    render::Vector3 to = pickPointAboveScreen(flight.area);
    if (flight.leavesTowardsCamera) {
        to = pickPointBehindCamera(flight.area);
    } else if (pickBetween(0.0f, 1.0f) < towardsPlanetChance) {
        to = pickPointBehindPlanet(flight.area, flight.hover);
    }
    const float speed = flight.leavesTowardsCamera ? returnSpeed : departureSpeed;
    setRoute(flight, FlightStage::Departure, flight.hover, control, to,
             computeRouteLength(flight.hover, control, to) / speed);
}

/// The route of the previous stage is kept while the ship waits: it stays where the route ended.
void FlightSystem::startNextStage(Flight &flight)
{
    switch (flight.stage) {
    case FlightStage::Approach:
        setStraightRoute(flight, FlightStage::Landing, flight.hover, flight.dock);
        break;
    case FlightStage::Landing:
        flight.stage = FlightStage::Docked;
        flight.duration = pickBetween(shortestStay, longestStay);
        break;
    case FlightStage::Docked:
        setStraightRoute(flight, FlightStage::TakeOff, flight.dock, flight.hover);
        break;
    case FlightStage::TakeOff:
        startDeparture(flight);
        break;
    case FlightStage::Departure:
        flight.stage = FlightStage::Away;
        flight.duration = pickBetween(shortestAbsence, longestAbsence);
        break;
    case FlightStage::Away:
        startApproach(flight);
        break;
    }
}

bool FlightSystem::isFlownByAnotherShip(ecs::entity_t ship, render::ModelId model) const
{
    for (ecs::entity_t entity = 0; entity < _flights.size(); ++entity) {
        if (entity == ship || !_flights.has(entity) || !_models.has(entity)) {
            continue;
        }
        if (_models.get(entity).model == model) {
            return true;
        }
    }
    return false;
}

/// Picks at random one of its models that neither this ship nor another one flies; keeps its model if there
/// is none.
void FlightSystem::changeShipModel(ecs::entity_t ship, Flight &flight)
{
    engine::Model &model = _models.get(ship);
    std::vector<ShipModel> freeModels;
    for (const ShipModel &candidate : flight.models) {
        if (candidate.model != model.model && !isFlownByAnotherShip(ship, candidate.model)) {
            freeModels.push_back(candidate);
        }
    }
    if (freeModels.empty()) {
        return;
    }
    std::uniform_int_distribution<std::size_t> pick(0, freeModels.size() - 1);
    const ShipModel &picked = freeModels[pick(_random)];
    model.model = picked.model;
    flight.animations = picked.animations;
}

void FlightSystem::update(float elapsed)
{
    for (ecs::entity_t entity = 0; entity < _flights.size(); ++entity) {
        if (!_flights.has(entity) || !_models.has(entity)) {
            continue;
        }
        Flight &flight = _flights.get(entity);
        const FlightStage previousStage = flight.stage;
        flight.time += std::min(elapsed, longestStep);
        while (flight.time >= flight.duration) {
            flight.time -= flight.duration;
            startNextStage(flight);
        }
        if (previousStage == FlightStage::Away && flight.stage != FlightStage::Away) {
            changeShipModel(entity, flight);
        }
        placeShip(_models.get(entity), flight);
        if (flight.stage != previousStage && _animations.has(entity)) {
            engine::switchModelAnimation(_animations.get(entity), selectAnimation(flight), animationFade);
        }
    }
}

} // namespace rtype::client
