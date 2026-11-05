#include "rtype/engine/System/ModelAnimationSystem.hpp"

namespace rtype::engine
{

namespace
{

// The animation added over the others goes on with them, and is removed once played: the entities left
// without one share their pose again
void playAddedAnimation(ModelAnimation &animation, float elapsed)
{
    if (animation.addedAnimation == NoAnimation) {
        return;
    }
    animation.addedTime += elapsed * animation.speed;
    if (animation.addedTime >= animation.addedDuration) {
        animation.addedAnimation = NoAnimation;
        animation.addedTime = 0.0f;
        animation.addedDuration = 0.0f;
    }
}

} // namespace

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
            playAddedAnimation(animation, elapsed);
        }
    }
}

} // namespace rtype::engine
