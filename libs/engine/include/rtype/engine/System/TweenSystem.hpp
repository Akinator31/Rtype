#pragma once

#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"

namespace rtype::engine
{

/// Advances the tweens and writes their value to the Scale, the Opacity or the Panel highlight of their
/// entity.
class TweenSystem : public ecs::System
{
    ecs::ComponentCache<Tween> &_tweens;
    ecs::ComponentCache<Scale> &_scales;
    ecs::ComponentCache<Opacity> &_opacities;
    ecs::ComponentCache<Panel> &_panels;

    void apply(ecs::entity_t entity, TweenTarget target, float value);

public:
    explicit TweenSystem(ecs::ComponentCaches &caches);

    void update(float elapsed) override;
};

} // namespace rtype::engine
