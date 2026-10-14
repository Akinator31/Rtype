#include "MainMenu/FlightSystem.hpp"

#include "MainMenu/Space.hpp"

#include <algorithm>
#include <cmath>

namespace rtype::client
{

namespace
{

/// Average speeds, in world units per second: the ships come in slowly, go down and up more slowly still,
/// and leave a little faster.
constexpr float approachSpeed = 1.8f;
constexpr float verticalSpeed = 0.5f;
constexpr float departureSpeed = 2.2f;
/// The ship that leaves towards the camera has a long way to go: it flies faster, for its trip not to last
/// too long, and rushes past the camera at the end.
constexpr float returnSpeed = 4.0f;

/// How far from above the dock a ship flies straight along its heading, when it arrives and when it leaves.
constexpr float runwayLength = 8.0f;

/// How long a ship stays on its dock, and out of the screen, in seconds.
constexpr float shortestStay = 4.0f;
constexpr float longestStay = 10.0f;
constexpr float shortestAbsence = 2.0f;
constexpr float longestAbsence = 8.0f;

/// The menu camera is at the origin and looks towards -z with a vertical field of view of 30 degrees: at a
/// depth d, the screen goes up and down to d * screenHalfHeight, and left and right to d * screenHalfWidth
/// on the widest screens. A point further than that by outOfScreenMargin, the size of a ship, is not seen.
constexpr float screenHalfHeight = 0.268f;
constexpr float screenHalfWidth = 0.48f;
constexpr float outOfScreenMargin = 4.0f;

/// The ships come from close to the camera, under the station, and leave far behind it.
constexpr float closestArrival = 45.0f;
constexpr float furthestArrival = 70.0f;
constexpr float closestExit = 80.0f;
constexpr float furthestExit = 130.0f;

/// Half of the ships come from behind the camera: they start just under it, out of the field of view, rise
/// into it big on the screen and fly away towards the station. They start in this box, where the ships that
/// leave towards the camera go back.
constexpr float fromCameraChance = 0.5f;
constexpr render::Vector3 behindCameraLow{-1.8f, -0.6f, 0.5f};
constexpr render::Vector3 behindCameraHigh{-1.0f, -0.5f, 1.0f};

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
render::Vector3 FlightSystem::pickPointBelowScreen()
{
    const float depth = pickBetween(closestArrival, furthestArrival);
    if (pickBetween(0.0f, 1.0f) < 0.5f) {
        return {depth * pickBetween(0.05f, screenHalfWidth), -(depth * screenHalfHeight) - outOfScreenMargin,
                -depth};
    }
    return {(depth * screenHalfWidth) + outOfScreenMargin, depth * pickBetween(-screenHalfHeight, 0.0f),
            -depth};
}

/// Over the top edge of the screen, or out of its right edge in its upper half.
render::Vector3 FlightSystem::pickPointAboveScreen()
{
    const float depth = pickBetween(closestExit, furthestExit);
    if (pickBetween(0.0f, 1.0f) < 0.5f) {
        return {depth * pickBetween(0.1f, screenHalfWidth), (depth * screenHalfHeight) + outOfScreenMargin,
                -depth};
    }
    return {(depth * screenHalfWidth) + outOfScreenMargin, depth * pickBetween(0.0f, screenHalfHeight),
            -depth};
}

render::Vector3 FlightSystem::pickPointBehindCamera()
{
    return {pickBetween(behindCameraLow.x, behindCameraHigh.x),
            pickBetween(behindCameraLow.y, behindCameraHigh.y),
            pickBetween(behindCameraLow.z, behindCameraHigh.z)};
}

void FlightSystem::startApproach(Flight &flight)
{
    const render::Vector3 from =
        pickBetween(0.0f, 1.0f) < fromCameraChance ? pickPointBehindCamera() : pickPointBelowScreen();
    const render::Vector3 control = flight.hover - (flight.heading * runwayLength);
    setRoute(flight, FlightStage::Approach, from, control, flight.hover,
             computeRouteLength(from, control, flight.hover) / approachSpeed);
}

void FlightSystem::startDeparture(Flight &flight)
{
    const render::Vector3 control = flight.hover + (flight.heading * runwayLength);
    const render::Vector3 to = flight.leavesTowardsCamera ? pickPointBehindCamera() : pickPointAboveScreen();
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
        placeShip(_models.get(entity), flight);
        if (flight.stage != previousStage && _animations.has(entity)) {
            engine::switchModelAnimation(_animations.get(entity), selectAnimation(flight), animationFade);
        }
    }
}

} // namespace rtype::client
