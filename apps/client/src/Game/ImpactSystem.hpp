#pragma once

#include "Game/ImpactLooks.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"

namespace rtype::client
{

/// Plays the impacts once: moves the time of their animation, fades their model out during the last part of
/// it, and destroys them at its end, before it would loop back to its start.
class ImpactSystem : public ecs::System
{
    ecs::Ecs &_ecs;
    ecs::ComponentCache<Impact> &_impacts;
    ecs::ComponentCache<engine::Model> &_models;
    ecs::ComponentCache<engine::ModelAnimation> &_animations;

    void playImpact(ecs::entity_t entity, Impact &impact);

public:
    /// Share of the animation, at its end, during which the impact fades out.
    static constexpr float FadeShare = 0.25f;

    ImpactSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs);

    void update(float elapsed) override;
};

} // namespace rtype::client
