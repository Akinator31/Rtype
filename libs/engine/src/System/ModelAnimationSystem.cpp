#include "rtype/engine/System/ModelAnimationSystem.hpp"

namespace rtype::engine
{

ModelAnimationSystem::ModelAnimationSystem(ecs::ComponentCaches &caches)
    : _animations(use<ModelAnimation>(caches))
{
}

void ModelAnimationSystem::update(float elapsed)
{
    for (ecs::entity_t entity = 0; entity < _animations.size(); ++entity) {
        if (_animations.has(entity)) {
            ModelAnimation &animation = _animations.get(entity);
            animation.time += elapsed * animation.speed;
        }
    }
}

} // namespace rtype::engine
