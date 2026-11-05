#pragma once

#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Animation.hpp"

namespace rtype::engine
{

/// Plays the animations of the models: moves the time of each ModelAnimation by the elapsed time. The
/// animations loop when they are drawn, so the time only grows. An added animation (addModelAnimation()) is
/// removed once it has played for its duration.
class ModelAnimationSystem : public ecs::System
{
    ecs::ComponentCache<ModelAnimation> &_animations;

public:
    explicit ModelAnimationSystem(ecs::ComponentCaches &caches);

    void update(float elapsed) override;
};

} // namespace rtype::engine
