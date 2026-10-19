#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/game/GameContext.hpp"
#include "rtype/game/Snapshot.hpp"

#include <cstdint>
#include <optional>

/// How the entities of the game are made and destroyed: every new kind of entity gets its function here, so
/// that the systems and the tests make them the same way.
namespace rtype::game
{

/// The ship of a player, at its place on the left of the game area: one height per slot.
ecs::entity_t spawnPlayerShip(ecs::Ecs &ecs, GameContext &context, std::uint8_t slot);

/// A shot of a player, going right from a position.
ecs::entity_t spawnPlayerShot(ecs::Ecs &ecs, GameContext &context, std::uint8_t slot, float x, float y);

/// A Bydo entering the game area from the right, at a random height.
ecs::entity_t spawnEnemy(ecs::Ecs &ecs, GameContext &context);

/// A shot of a Bydo, going left from a position.
ecs::entity_t spawnEnemyShot(ecs::Ecs &ecs, GameContext &context, float x, float y);

/// The ship of a player, if it has one.
[[nodiscard]] std::optional<ecs::entity_t> findPlayerShip(ecs::Ecs &ecs, std::uint8_t slot);

/// Destroys an entity and records it for the next snapshot, with the cause. Does nothing if it is not alive.
/// Inside a system, call it through Ecs::defer(): destroying while a system goes through the caches is not
/// safe.
void destroyEntity(ecs::Ecs &ecs, GameContext &context, ecs::entity_t entity, DestroyCause cause);

} // namespace rtype::game
