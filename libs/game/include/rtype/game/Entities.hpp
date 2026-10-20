#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/game/GameContext.hpp"
#include "rtype/game/Snapshot.hpp"

#include <cstdint>
#include <optional>
#include <string>

/// How the entities of the game are made and destroyed: every new kind of entity gets its function here, so
/// that the systems and the tests make them the same way.
namespace rtype::game
{

/// The ship of a player, at its place on the left of the game area: one height per slot.
ecs::entity_t spawnPlayerShip(ecs::Ecs &ecs, GameContext &context, std::uint8_t slot);

/// A shot of a player, going right from a position.
ecs::entity_t spawnPlayerShot(ecs::Ecs &ecs, GameContext &context, std::uint8_t slot, float x, float y);

/// A projectile of a registered kind, from a position, with a velocity (game units per second). It belongs to
/// a player if `owner` is a slot, to nobody if it is NoOwner. It is removed once out of the game area.
/// Throws std::out_of_range if `id` is not in context.projectiles.
ecs::entity_t spawnProjectile(ecs::Ecs &ecs, GameContext &context, const std::string &id, std::uint8_t owner,
                              float x, float y, float velocityX, float velocityY);

/// Gives a weapon of context.weapons to a ship, with a full magazine (it replaces the one it had). From then
/// on the WeaponSystem makes it fire instead of the default shots. Throws std::out_of_range if `weaponId` is
/// not in context.weapons.
void equipWeapon(ecs::Ecs &ecs, GameContext &context, ecs::entity_t ship, const std::string &weaponId);

/// A shot of a weapon: its projectile, fired to the right at the speed of the weapon, with the firing
/// mechanic and the range of the weapon. `x` and `y` are the position of the ship, the shot appears at its
/// nose. Throws std::out_of_range if the weapon or its projectile is unknown.
ecs::entity_t spawnWeaponShot(ecs::Ecs &ecs, GameContext &context, const std::string &weaponId,
                              std::uint8_t owner, float x, float y);

/// The ship of a player, if it has one.
[[nodiscard]] std::optional<ecs::entity_t> findPlayerShip(ecs::Ecs &ecs, std::uint8_t slot);

/// Destroys an entity and records it for the next snapshot, with the cause. Does nothing if it is not alive.
/// Inside a system, call it through Ecs::defer(): destroying while a system goes through the caches is not
/// safe.
void destroyEntity(ecs::Ecs &ecs, GameContext &context, ecs::entity_t entity, DestroyCause cause);

} // namespace rtype::game
