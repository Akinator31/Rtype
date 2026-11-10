#include "rtype/game/System/ItemSystem.hpp"

#include "rtype/game/Entities.hpp"
#include "rtype/game/Hitbox.hpp"
#include "rtype/game/Item/ItemEffects.hpp"

#include <vector>

namespace rtype::game
{

ItemSystem::ItemSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context)
    : _ecs(ecs), _context(context), _items(use<Item>(caches)), _ships(use<PlayerShip>(caches)),
      _healths(use<Health>(caches)), _positions(use<ecs::Position>(caches)),
      _colliders(use<physics::Collider>(caches))
{
}

void ItemSystem::update(float)
{
    std::vector<ecs::entity_t> items;
    for (ecs::entity_t entity = 0; entity < _items.size(); ++entity) {
        if (_items.has(entity) && _ecs.isAlive(entity)) {
            items.push_back(entity);
        }
    }
    for (const ecs::entity_t entity : items) {
        Item &item = _items.get(entity);
        ++item.age;
        if (item.age >= item.lifetime) {
            destroyLater(entity, DestroyCause::Removed);
            continue;
        }
        if (item.age < _context.rules.itemAppearTicks || _context.rescue.stage != RescueStage::None) {
            continue;
        }
        if (const std::optional<ecs::entity_t> ship = findPicker(entity)) {
            pickUp(entity, *ship);
        }
    }
}

std::optional<ecs::entity_t> ItemSystem::findPicker(ecs::entity_t item) const
{
    if (!_positions.has(item) || !_colliders.has(item)) {
        return std::nullopt;
    }
    std::optional<ecs::entity_t> picker;
    std::uint8_t slot = MaxPlayers;
    for (ecs::entity_t ship = 0; ship < _ships.size(); ++ship) {
        if (!_ships.has(ship) || !_ecs.isAlive(ship) || !_positions.has(ship) || !_colliders.has(ship) ||
            !_healths.has(ship) || _healths.get(ship).points == 0 || _ships.get(ship).slot >= slot) {
            continue;
        }
        if (overlaps(_positions.get(item), _colliders.get(item), _positions.get(ship),
                     _colliders.get(ship))) {
            picker = ship;
            slot = _ships.get(ship).slot;
        }
    }
    return picker;
}

void ItemSystem::pickUp(ecs::entity_t item, ecs::entity_t ship)
{
    destroyLater(item, DestroyCause::Killed);
    const ItemDefinition &definition = _context.items.getDefinition(_items.get(item).id);
    for (const ItemEffect &effect : definition.effects) {
        if (!applyItemEffect(_ecs, _context, ship, effect)) {
            break;
        }
    }
}

void ItemSystem::destroyLater(ecs::entity_t entity, DestroyCause cause)
{
    const ecs::EntityRef reference = _ecs.ref(entity);
    _ecs.defer([this, reference, cause] {
        if (_ecs.isAlive(reference)) {
            destroyEntity(_ecs, _context, reference.id, cause);
        }
    });
}

} // namespace rtype::game
