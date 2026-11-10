#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/game/GameContext.hpp"
#include "rtype/game/Item/ItemDefinition.hpp"

namespace rtype::game
{

/// Applies one effect of an item to the ship of a player that picked it up (ItemEffectType tells what each
/// does). A ship that Damage leaves without hit points is destroyed (DestroyCause::Killed) and its player
/// loses a life, as in CombatSystem.
///
/// The components it adds (Invulnerable) and the ship it destroys are added and destroyed after the systems
/// of the tick, through Ecs::defer(): call it from a system.
/// @return false once the ship is lost: the next effects of the item are not applied.
bool applyItemEffect(ecs::Ecs &ecs, GameContext &context, ecs::entity_t ship, const ItemEffect &effect);

} // namespace rtype::game
