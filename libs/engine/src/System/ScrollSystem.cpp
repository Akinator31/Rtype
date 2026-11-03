#include "rtype/engine/System/ScrollSystem.hpp"

namespace rtype::engine
{

ScrollSystem::ScrollSystem(ecs::ComponentCaches &caches) : _scrolls(use<Scroll>(caches))
{
}

void ScrollSystem::update(float elapsed)
{
    for (ecs::entity_t entity = 0; entity < _scrolls.size(); ++entity) {
        if (_scrolls.has(entity)) {
            Scroll &scroll = _scrolls.get(entity);
            scroll.offset += scroll.speed * elapsed;
        }
    }
}

} // namespace rtype::engine
