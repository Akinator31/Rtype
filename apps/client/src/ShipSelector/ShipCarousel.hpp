#pragma once

#include "ShipSelector/ShipSelection.hpp"
#include "rtype/ecs/Entity/Entity.hpp"

#include <vector>

namespace rtype::client
{

/// The carousel of the hangar, on an entity of its own: the ShipSelectorSystem glides it towards the ship
/// looked at, places the ship models on its sides, turns the ship in the middle when the player drags the
/// mouse, and lights the dots of the pagination.
struct ShipCarousel {
    /// Where the ship looked at is read, and the glide written. It must outlive the entity.
    ShipSelection *selection = nullptr;
    /// The entity with the Model of each ship of the catalog, in the same order.
    std::vector<ecs::entity_t> ships;
    /// How much each ship is turned, in degrees, from its resting pose: the player turns one at a time.
    std::vector<float> yaws;
    /// The dot of each ship in the pagination, in the same order.
    std::vector<ecs::entity_t> dots;
    /// The texts that fade out and in while the carousel glides: the name, the button.
    std::vector<ecs::entity_t> fading;
    /// The player holds the mouse button to turn the ship, and where the pointer was at the previous update.
    bool dragging = false;
    float lastPointerX = 0.0f;
};

} // namespace rtype::client
