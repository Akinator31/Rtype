#pragma once

#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <vector>

namespace rtype::client
{

/// What a ship of the menu is doing. The stages follow each other in this order, then start again.
enum class FlightStage {
    /// Flies from out of the screen to above its dock, slowing down.
    Approach,
    /// Goes straight down onto its dock.
    Landing,
    /// Stays on its dock.
    Docked,
    /// Goes straight up from its dock.
    TakeOff,
    /// Flies from above its dock out of the screen, faster and faster.
    Departure,
    /// Waits out of the screen before coming back.
    Away,
};

/// The animations of the ship model, by index (render::ResourceManager::getModelAnimation()): small flame
/// when it hovers or rests, big flame when it flies, nose up or down when it climbs or goes down.
struct ShipAnimations {
    int idle = 0;
    int move = 0;
    int moveUp = 0;
    int moveDown = 0;
};

/// A ship model the menu can fly, with its animations.
struct ShipModel {
    render::ModelId model;
    ShipAnimations animations;
};

/// A ship of the menu going back and forth between its dock on the station and the space out of the screen.
/// The FlightSystem moves its Model along the route of the stage: nose forward while it flies, along
/// `heading` while it goes down, stays and goes up. Each stage plays its animation on the ModelAnimation of
/// the entity, if it has one. Each time the ship comes back, it is another of `models`, one that no other
/// ship flies.
///
/// A route is a curve from `from` to `to`, pulled towards `control`: the ship leaves `from` heading towards
/// `control`, and reaches `to` coming from it.
struct Flight {
    /// Where the ship lands on the station, in world units.
    render::Vector3 dock;
    /// Where it stops above the dock before going down, and goes up to before leaving.
    render::Vector3 hover;
    /// Direction the nose of the ship points to from above the dock to the dock and back: it arrives and
    /// leaves in this direction. A unit vector.
    render::Vector3 heading;
    /// Where the top of the ship faces while it flies: the top of the station. A unit vector.
    render::Vector3 up;
    /// The models the ship can be, and the animations of the one it is.
    std::vector<ShipModel> models;
    ShipAnimations animations;
    /// Whether the ship leaves towards the camera, passing under it, instead of out of the top or right edge
    /// of the screen.
    bool leavesTowardsCamera = false;
    FlightStage stage = FlightStage::Away;
    /// Time spent in the stage, and how long it lasts, in seconds.
    float time = 0.0f;
    float duration = 0.0f;
    render::Vector3 from;
    render::Vector3 control;
    render::Vector3 to;
};

} // namespace rtype::client
