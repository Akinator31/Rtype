#include "rtype/game/Item/ItemEffects.hpp"

#include "rtype/game/Component/Components.hpp"
#include "rtype/game/Entities.hpp"

#include <algorithm>
#include <cmath>

namespace rtype::game
{

namespace
{

std::uint32_t toTicks(float seconds)
{
    return std::max<std::uint32_t>(
        1, static_cast<std::uint32_t>(std::lround(seconds * static_cast<float>(TickRate))));
}

PlayerSlot &playerOf(ecs::Ecs &ecs, GameContext &context, ecs::entity_t ship)
{
    return context.players[ecs.getComponent<PlayerShip>(ship).slot];
}

bool isProtected(ecs::Ecs &ecs, ecs::entity_t ship)
{
    return ecs.hasComponent<Invulnerable>(ship) && ecs.getComponent<Invulnerable>(ship).ticks > 0;
}

void heal(ecs::Ecs &ecs, GameContext &context, ecs::entity_t ship, std::uint8_t amount)
{
    Health &health = ecs.getComponent<Health>(ship);
    const unsigned healed = static_cast<unsigned>(health.points) + amount;
    const unsigned most = context.maxHealth(playerOf(ecs, context, ship));
    health.points = static_cast<std::uint8_t>(std::min(healed, most));
}

void addLives(ecs::Ecs &ecs, GameContext &context, ecs::entity_t ship, std::uint8_t amount)
{
    PlayerSlot &player = playerOf(ecs, context, ship);
    player.lives = static_cast<std::uint8_t>(std::min<unsigned>(player.lives + amount, 0xFF));
}

/// Returns false if the ship is lost.
bool hurt(ecs::Ecs &ecs, GameContext &context, ecs::entity_t ship, std::uint8_t amount)
{
    if (isProtected(ecs, ship)) {
        return true;
    }
    Health &health = ecs.getComponent<Health>(ship);
    health.points -= std::min(health.points, amount);
    if (health.points > 0) {
        return true;
    }
    context.loseLife(playerOf(ecs, context, ship));
    const ecs::EntityRef reference = ecs.ref(ship);
    ecs.defer([&ecs, &context, reference] {
        if (ecs.isAlive(reference)) {
            destroyEntity(ecs, context, reference.id, DestroyCause::Killed);
        }
    });
    return false;
}

void boostFire(ecs::Ecs &ecs, GameContext &context, ecs::entity_t ship, const ItemEffect &effect)
{
    if (!ecs.hasComponent<Armed>(ship)) {
        return;
    }
    Armed &armed = ecs.getComponent<Armed>(ship);
    const WeaponDefinition &weapon = context.weapons.getDefinition(armed.weapon);
    const float interval = static_cast<float>(weapon.fireInterval) / effect.factor;
    armed.boostInterval = std::max<std::uint32_t>(1, static_cast<std::uint32_t>(std::lround(interval)));
    armed.boostTicks = toTicks(effect.duration);
    // Ready at once at the new pace
    armed.cooldown = std::min(armed.cooldown, armed.boostInterval);
}

void shield(ecs::Ecs &ecs, ecs::entity_t ship, float duration)
{
    const std::uint32_t ticks = toTicks(duration);
    if (ecs.hasComponent<Invulnerable>(ship)) {
        Invulnerable &invulnerable = ecs.getComponent<Invulnerable>(ship);
        invulnerable.ticks = std::max(invulnerable.ticks, ticks);
        return;
    }
    // Added after the systems: it could grow a cache being read
    const ecs::EntityRef reference = ecs.ref(ship);
    ecs.defer([&ecs, reference, ticks] {
        if (!ecs.isAlive(reference)) {
            return;
        }
        if (ecs.hasComponent<Invulnerable>(reference.id)) {
            Invulnerable &invulnerable = ecs.getComponent<Invulnerable>(reference.id);
            invulnerable.ticks = std::max(invulnerable.ticks, ticks);
        } else {
            ecs.addComponent<Invulnerable>(reference.id, ticks);
        }
    });
}

} // namespace

bool applyItemEffect(ecs::Ecs &ecs, GameContext &context, ecs::entity_t ship, const ItemEffect &effect)
{
    switch (effect.type) {
    case ItemEffectType::Heal:
        heal(ecs, context, ship, effect.amount);
        break;
    case ItemEffectType::ExtraLife:
        addLives(ecs, context, ship, effect.amount);
        break;
    case ItemEffectType::Damage:
        return hurt(ecs, context, ship, effect.amount);
    case ItemEffectType::RapidFire:
        boostFire(ecs, context, ship, effect);
        break;
    case ItemEffectType::Shield:
        shield(ecs, ship, effect.duration);
        break;
    }
    return true;
}

} // namespace rtype::game
