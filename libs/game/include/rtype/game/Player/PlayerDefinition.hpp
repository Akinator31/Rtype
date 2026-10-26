#pragma once

#include <cstdint>
#include <string>

namespace rtype::game
{

/// What every player starts with: its ship and its weapon. loadGameConfig() reads it from config/player.lua
/// (ADR 0018). Left empty, as in a GameConfig made by hand, the ships have no weapon and cannot fire.
struct PlayerDefinition {
    /// Its ship (model and sounds): the id of a ShipDefinition of the ShipRegistry. Its size is that of the
    /// rules (Rules::shipHalfWidth), shared with the prediction of the client.
    std::string ship;
    /// The id of a WeaponDefinition of the WeaponRegistry.
    std::string weapon;
    /// Hit points of each ship (not 0), the same unit as the damage of the weapons.
    std::uint8_t health = 100;
};

} // namespace rtype::game
