#pragma once

#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Animation.hpp"

namespace rtype::engine
{

/// Scrolls the entities that have a Scroll: adds their speed times the elapsed time to their offset. The
/// SceneRenderer slides their Backdrop or BackdropSprite by this offset.
class ScrollSystem : public ecs::System
{
    ecs::ComponentCache<Scroll> &_scrolls;

public:
    explicit ScrollSystem(ecs::ComponentCaches &caches);

    void update(float elapsed) override;
};

} // namespace rtype::engine
