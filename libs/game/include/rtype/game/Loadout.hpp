#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace rtype::game
{

/// Weapons a player takes into a game.
inline constexpr std::size_t LoadoutSize = 2;

/// The weapons a player chose in the hangar, by the ids of their WeaponDefinition: the first is in hand when
/// its ship appears, the second put away (Holster) until the player switches to it
/// (PlayerInput::SecondWeapon). Empty, the ship has the weapon of the PlayerDefinition alone.
using Loadout = std::array<std::string, LoadoutSize>;

/// A weapon of the hangar of the client and the level of the account from which a player may take it into a
/// game, read from config/hangar.lua. Only the weapons of the hangar, and that of the PlayerDefinition, can
/// be in a loadout (Simulation::setLoadout()).
struct WeaponUnlock {
    /// The id of a WeaponDefinition.
    std::string weapon;
    /// From 1: a weapon of level 1 is open to every player.
    std::uint32_t level = 1;
};

} // namespace rtype::game
