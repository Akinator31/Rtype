#pragma once

#include "rtype/game/Ship/ShipSounds.hpp"

#include <string>

namespace rtype::game
{

/// Everything that makes a kind of ship, whoever flies it: creating a new one is filling this and registering
/// it in a ShipRegistry (ADR 0018 will load it from a file).
struct ShipDefinition {
    /// Unique name in a registry, e.g. "bydo_scout".
    std::string id;
    /// Path of the 3D model, drawn by the client. The game only carries it: it never loads it.
    std::string model;
    ShipSounds sounds;
};

} // namespace rtype::game
