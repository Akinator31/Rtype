#include "Game/ShotPrediction.hpp"

#include "rtype/game/Constants.hpp"

#include <algorithm>

namespace rtype::client
{

bool ShotPrediction::isShotOf(const game::EntityState &entity, std::uint8_t slot)
{
    return entity.owner == slot && (entity.kind == game::EntityKind::PlayerShot ||
                                    entity.kind == game::EntityKind::PlayerChargedShot);
}

void ShotPrediction::destroyed(std::uint32_t id, std::uint32_t tick)
{
    _ends.try_emplace(id, End{tick, false});
}

void ShotPrediction::apply(game::Snapshot &snapshot, std::uint8_t slot, double drawn, double ship)
{
    const auto ended = [&](const game::EntityState &entity) {
        const auto end = _ends.find(entity.id);
        return end != _ends.end() && end->second.shown && ship >= end->second.tick;
    };
    std::erase_if(snapshot.entities,
                  [&](const game::EntityState &entity) { return isShotOf(entity, slot) && ended(entity); });

    for (game::EntityState &entity : snapshot.entities) {
        if (!isShotOf(entity, slot)) {
            continue;
        }
        double target = ship;
        if (const auto end = _ends.find(entity.id); end != _ends.end() && ship >= end->second.tick) {
            // Where the game destroyed it, once: the impact is drawn there
            target = end->second.tick;
            end->second.shown = true;
        }
        const auto seconds = static_cast<float>(std::max(0.0, target - drawn) / game::TickRate);
        entity.x += entity.velocityX * seconds;
        entity.y += entity.velocityY * seconds;
    }
    // The interpolation no longer draws them once the drawn tick passes their destruction
    std::erase_if(
        _ends, [drawn](const auto &entry) { return static_cast<double>(entry.second.tick) < drawn - 1.0; });
}

} // namespace rtype::client
