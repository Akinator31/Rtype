#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/GameContext.hpp"
#include "rtype/physics/Component/Collider.hpp"

namespace rtype::game
{

/// The items of the game (ItemDefinition), once they have appeared (Rules::itemAppearTicks): the ship of a
/// player whose hitbox touches one picks it up, gets its effects in order (applyItemEffect()), and the item
/// is destroyed (DestroyCause::Killed, the client shows it picked up). An item touched by several ships at
/// once goes to the first player. An item nobody picks up vanishes after its lifetime
/// (DestroyCause::Removed). During the rescue against the final boss, nothing is picked up.
///
/// The entities are destroyed after the systems of the tick.
class ItemSystem : public ecs::System
{
    ecs::Ecs &_ecs;
    GameContext &_context;
    ecs::ComponentCache<Item> &_items;
    ecs::ComponentCache<PlayerShip> &_ships;
    ecs::ComponentCache<Health> &_healths;
    ecs::ComponentCache<ecs::Position> &_positions;
    ecs::ComponentCache<physics::Collider> &_colliders;

    /// The ship of a player alive that touches an item, if any.
    [[nodiscard]] std::optional<ecs::entity_t> findPicker(ecs::entity_t item) const;
    void pickUp(ecs::entity_t item, ecs::entity_t ship);
    void destroyLater(ecs::entity_t entity, DestroyCause cause);

public:
    ItemSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context);

    void update(float elapsed) override;
};

} // namespace rtype::game
