#pragma once

#include "rtype/game/Ship/ShipSounds.hpp"
#include "rtype/physics/Component/Collider.hpp"

#include <string>

namespace rtype::game
{

/// Everything that makes a kind of ship, whoever flies it: creating a new one is filling this and registering
/// it in a ShipRegistry; loadGameConfig() reads them from config/ships/ (ADR 0018).
struct ShipDefinition {
    /// Unique name in a registry, e.g. "bydo_scout".
    std::string id;
    /// Path of the 3D model, drawn by the client. The game only carries it: it never loads it.
    std::string model;
    ShipSounds sounds;
    /// The shape that hits and is hit, centred on the ship. The ships of the players keep the box of the
    /// rules (Rules::shipHalfWidth), shared with the prediction of the client.
    physics::Collider collider{physics::Aabb{{}, {48.0f, 24.0f, 0.0f}}, {}, false};
};

} // namespace rtype::game
