#pragma once

#include <cstddef>
#include <cstdint>

namespace rtype::game
{

/// Width of the game area, in game units: the area the client shows, whatever its window
/// (docs/protocol/game.md, section 9.1). The origin is the top left corner, x goes right and y down.
inline constexpr float GameWidth = 1920.0f;
/// Height of the game area, in game units.
inline constexpr float GameHeight = 1080.0f;

/// Ticks per second: the simulation always steps by TickDuration, whatever the frame rate.
inline constexpr std::uint8_t TickRate = 60;
/// Duration of one tick, in seconds.
inline constexpr float TickDuration = 1.0f / TickRate;

/// Most players in one game: slots 0 to 3.
inline constexpr std::size_t MaxPlayers = 4;

/// The owner of an entity that belongs to no player (a Bydo, its shots).
inline constexpr std::uint8_t NoOwner = 0xFF;

/// Positions the protocol can encode, in game units: entities must stay strictly inside, so the game removes
/// them well before (see LeavesArea).
inline constexpr float PositionLimit = 4096.0f;

} // namespace rtype::game
