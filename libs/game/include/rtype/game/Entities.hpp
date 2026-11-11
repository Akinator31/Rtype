#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/game/GameContext.hpp"
#include "rtype/game/Snapshot.hpp"
#include "rtype/game/Weapon/WeaponFiring.hpp"

#include <cstdint>
#include <optional>
#include <string>

/// How the entities of the game are made and destroyed: every new kind of entity gets its function here, so
/// that the systems and the tests make them the same way.
namespace rtype::game
{

/// Where the ship of a player appears: on the left of the game area, one height per slot.
[[nodiscard]] ecs::Position getShipSpawn(std::uint8_t slot);

/// The ship of a player, at its place (getShipSpawn()). It has the health of context.player, and the weapons
/// of the loadout of its player (equipLoadout()).
ecs::entity_t spawnPlayerShip(ecs::Ecs &ecs, GameContext &context, std::uint8_t slot);

/// An ally of the rescue against the final boss (RescueSystem), at a position, still: an AllyShip that
/// belongs to no player, without health nor hitbox, as nothing hits it. The RescueSystem flies it, and fires
/// its shots.
ecs::entity_t spawnAlly(ecs::Ecs &ecs, GameContext &context, std::uint8_t index, float x, float y);

/// A Bydo of context.enemies at a position, still: the EnemySystem moves it, with the behavior of its
/// definition if it has one (then with EnemyOrders), to the left otherwise. It has the hitbox of its ship,
/// the health and the weapon of its definition, and hurts the ships it runs into. It is removed once out of
/// the game area. Throws std::out_of_range if `enemyId`, its ship or its weapon is unknown.
ecs::entity_t spawnEnemy(ecs::Ecs &ecs, GameContext &context, const std::string &enemyId, float x, float y);

/// A projectile of a registered kind, from a position, with a velocity (game units per second). It belongs to
/// a player if `owner` is a slot, to nobody if it is NoOwner. It is removed once out of the game area.
/// Throws std::out_of_range if `id` is not in context.projectiles.
ecs::entity_t spawnProjectile(ecs::Ecs &ecs, GameContext &context, const std::string &id, std::uint8_t owner,
                              float x, float y, float velocityX, float velocityY);

/// An item of context.items at a position, drifting to the left at its speed: the ships of the players that
/// touch it pick it up (ItemSystem), once it has appeared (Rules::itemAppearTicks). It vanishes after its
/// lifetime, or once out of the game area. Throws std::out_of_range if `itemId` is unknown.
ecs::entity_t spawnItem(ecs::Ecs &ecs, GameContext &context, const std::string &itemId, float x, float y);

/// Gives a weapon of context.weapons to a ship, with a full magazine (it replaces the one it had). From then
/// on the WeaponSystem makes it fire. Throws std::out_of_range if `weaponId` is not in context.weapons.
void equipWeapon(ecs::Ecs &ecs, GameContext &context, ecs::entity_t ship, const std::string &weaponId);

/// Gives the weapons of a loadout to the ship of a player, magazines full: the first in hand, the second in
/// its Holster. Empty, the ship has the weapon of context.player alone (no weapon if its id is empty), and
/// no Holster. Throws std::out_of_range if a weapon is not in context.weapons.
void equipLoadout(ecs::Ecs &ecs, GameContext &context, ecs::entity_t ship, const Loadout &loadout);

/// A shot of a weapon: its projectile, fired forward at the speed of the weapon, with the firing mechanic,
/// the range and the damage of the weapon. Forward is to the right for a player, to the left for a Bydo
/// (NoOwner). `x` and `y` are the nose of the ship: the shot appears just in front of it. Throws
/// std::out_of_range if the weapon or its projectile is unknown.
ecs::entity_t spawnWeaponShot(ecs::Ecs &ecs, GameContext &context, const std::string &weaponId,
                              std::uint8_t owner, float x, float y);

/// Same as spawnWeaponShot(), fired towards a point instead of forward: the shot flies along the line from
/// the nose to the point, and goes past it. A point on the nose itself fires forward.
ecs::entity_t spawnAimedShot(ecs::Ecs &ecs, GameContext &context, const std::string &weaponId,
                             std::uint8_t owner, float x, float y, float targetX, float targetY);

/// The shot of a weapon from where and at the velocity a ShotLaunch says (aimShot(), playerShot()), with the
/// firing mechanic, the range and the damage of the weapon. Throws std::out_of_range if the weapon or its
/// projectile is unknown.
ecs::entity_t spawnShot(ecs::Ecs &ecs, GameContext &context, const std::string &weaponId, std::uint8_t owner,
                        const ShotLaunch &launch);

/// The ship of a player, if it has one.
[[nodiscard]] std::optional<ecs::entity_t> findPlayerShip(ecs::Ecs &ecs, std::uint8_t slot);

/// Destroys an entity and records it for the next snapshot, with the cause. Does nothing if it is not alive.
/// Inside a system, call it through Ecs::defer(): destroying while a system goes through the caches is not
/// safe.
void destroyEntity(ecs::Ecs &ecs, GameContext &context, ecs::entity_t entity, DestroyCause cause);

} // namespace rtype::game
