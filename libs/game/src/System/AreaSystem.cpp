#include "rtype/game/System/AreaSystem.hpp"

#include "rtype/game/Entities.hpp"

namespace rtype::game
{

AreaSystem::AreaSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context)
    : _ecs(ecs), _context(context), _leaving(use<LeavesArea>(caches)), _positions(use<ecs::Position>(caches))
{
}

void AreaSystem::update(float)
{
    const float margin = _context.rules.areaMargin;
    for (ecs::entity_t entity = 0; entity < _leaving.size(); ++entity) {
        if (!_leaving.has(entity) || !_positions.has(entity) || !_ecs.isAlive(entity)) {
            continue;
        }
        const LeavesArea &size = _leaving.get(entity);
        const ecs::Position &position = _positions.get(entity);
        const bool outside =
            position.x + size.halfWidth < -margin || position.x - size.halfWidth > GameWidth + margin ||
            position.y + size.halfHeight < -margin || position.y - size.halfHeight > GameHeight + margin;
        if (outside) {
            const ecs::EntityRef reference = _ecs.ref(entity);
            _ecs.defer([this, reference] {
                if (_ecs.isAlive(reference)) {
                    destroyEntity(_ecs, _context, reference.id, DestroyCause::Removed);
                }
            });
        }
    }
}

} // namespace rtype::game
