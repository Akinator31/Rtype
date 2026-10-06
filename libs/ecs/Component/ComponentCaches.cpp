#include "ecs/Component/ComponentCaches.hpp"

namespace ecs {

void ComponentCaches::erase(entity_t entity) {
    for (auto& [type, cache] : _caches) {
        cache->erase(entity);
    }
}

} // namespace ecs
