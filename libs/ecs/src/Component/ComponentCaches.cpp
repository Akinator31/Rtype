#include "rtype/ecs/Component/ComponentCaches.hpp"

namespace rtype::ecs
{

void ComponentCaches::erase(entity_t entity)
{
    for (auto &[type, cache] : _caches) {
        cache->erase(entity);
    }
}

} // namespace rtype::ecs
