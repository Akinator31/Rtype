#include "rtype/engine/System/BoundTextSystem.hpp"

namespace rtype::engine
{

BoundTextSystem::BoundTextSystem(ecs::ComponentCaches &caches)
    : _texts(use<Text>(caches)), _bindings(use<BoundText>(caches))
{
}

void BoundTextSystem::update(float /*elapsed*/)
{
    for (ecs::entity_t entity = 0; entity < _bindings.size(); ++entity) {
        if (_bindings.has(entity) && _texts.has(entity) && _bindings.get(entity).get) {
            _texts.get(entity).text = _bindings.get(entity).get();
        }
    }
}

} // namespace rtype::engine
