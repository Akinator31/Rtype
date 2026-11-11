#pragma once

#include "ShipModel.hpp"
#include "ShipSelector/ShipSelection.hpp"
#include "rtype/ecs/Entity/Entity.hpp"
#include "rtype/render/Types.hpp"

#include <functional>
#include <optional>
#include <vector>

namespace rtype::client
{

/// The carousel of the hangar, on an entity of its own: the ShipSelectorSystem glides it towards the ship
/// looked at, places the ship models on its sides, turns the ship in the middle when the player drags the
/// mouse, and lights the dots of the pagination. Before a game, it plays the take-off of the ship picked.
struct ShipCarousel {
    /// Where the ship looked at is read, and the glide written. It must outlive the entity.
    ShipSelection *selection = nullptr;
    /// The entity with the Model of each ship of the catalog, in the same order.
    std::vector<ecs::entity_t> ships;
    /// The animations of each ship, in the same order: the one picked takes off with its big flame, then
    /// cruises with the little flame of the game.
    std::vector<ShipAnimations> animations;
    /// The pedestal the ship floats over: it sinks out of the screen as the ship takes off.
    ecs::entity_t pedestal = 0;
    /// How much each ship is turned, in degrees, from its resting pose: the player turns one at a time.
    std::vector<float> yaws;
    /// The dot of each ship in the pagination, in the same order.
    std::vector<ecs::entity_t> dots;
    /// The texts that fade out and in while the carousel glides: the name, the button.
    std::vector<ecs::entity_t> fading;
    /// The player holds the mouse button to turn the ship, and where the pointer was at the previous update.
    bool dragging = false;
    float lastPointerX = 0.0f;

    /// The interface of the hangar, which fades out as the ship takes off.
    std::vector<ecs::entity_t> interface;
    /// The entity with the Camera and the Light of the hangar, which follow the ship as it takes off, and
    /// the one with its starry sky, which gives way to the sky of the game.
    ecs::entity_t camera = 0;
    ecs::entity_t sky = 0;
    /// Seconds since the ship picked began to take off (ShipSelection::departing), none before; where it was
    /// then, and how much it was turned, in degrees, and the height of the pedestal; the view and the light
    /// of the hangar then.
    std::optional<float> departure;
    render::Vector3 launchPad;
    float departureYaw = 0.0f;
    float pedestalHeight = 0.0f;
    render::Camera hangarView;
    render::Light hangarLight;
    /// Where the ship is, once it flies by itself.
    render::Vector3 flightPosition;
    /// Seconds since the sky of the game was opened in the hangar, none before.
    std::optional<float> skyAge;
    /// Seconds since the game was launched, none before: the hangar is closed when the game opens.
    std::optional<float> launched;

    /// Whether the game loaded (Engine::isSceneReady()): the ship flies until it is, so that the game opens
    /// at once, without the loading scene. Empty: it is not waited for.
    std::function<bool()> isArrivalReady;
    /// Creates the sky of the game in the hangar (createGameSky()), behind its starry sky.
    std::function<void()> openGameSky;
    /// Called once, when the ship flies in the sky of the game, framed as the game will open: launches the
    /// game, which closes the hangar as it opens; the ship flies on until then.
    std::function<void()> onDeparted;
    /// Called if the game has not opened long after it was launched: closes the hangar.
    std::function<void()> onLost;
};

} // namespace rtype::client
