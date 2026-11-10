#pragma once

#include <cstdint>
#include <string>

namespace rtype::game
{

/// A ship of the hangar of the client and the level of the account from which a player may fly it, read from
/// config/hangar.lua. The ships of the hangar are those of the client, named by their code ("VX-01"): the
/// game does not know them, it only reads the table for the client.
struct ShipUnlock {
    std::string ship;
    /// From 1: a ship of level 1 is open to every player.
    std::uint32_t level = 1;
};

} // namespace rtype::game
