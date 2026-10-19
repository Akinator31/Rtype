#pragma once

#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/game/PlayerInput.hpp"
#include "rtype/game/Rules.hpp"
#include "rtype/physics/Component/Motion.hpp"

/// How the ship of a player moves, in one place: the game uses it in its systems, and the client to predict
/// the ship of its player online (docs/architecture/client-online.md). Both must give the same position, to
/// the bit, or the prediction would be corrected at every snapshot.
namespace rtype::game
{

/// The velocity of a ship for an input: Rules::shipSpeed in the direction held, as fast in diagonal.
[[nodiscard]] physics::Velocity shipVelocity(PlayerInput input, const Rules &rules);

/// The position of a ship after one tick with this input: moved by its velocity for TickDuration, then kept
/// entirely in the game area. The same operations as PlayerControlSystem, physics::MovementSystem and
/// physics::BoundsSystem, in the same order.
[[nodiscard]] ecs::Position moveShip(ecs::Position position, PlayerInput input, const Rules &rules);

} // namespace rtype::game
