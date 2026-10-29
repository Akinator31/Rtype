#pragma once

#include "Game/DamageLooks.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"

namespace rtype::client
{

/// Shows the numbers of damage for a while: moves them up, fades them out during the last part of their time,
/// and destroys them at its end.
class DamageNumberSystem : public ecs::System
{
    ecs::Ecs &_ecs;
    ecs::ComponentCache<DamageNumber> &_numbers;
    ecs::ComponentCache<ecs::Position> &_positions;
    ecs::ComponentCache<engine::Opacity> &_opacities;

    void showNumber(ecs::entity_t entity, const DamageNumber &number, float elapsed);

public:
    /// How fast a number rises, in game units per second.
    static constexpr float RiseSpeed = 90.0f;
    /// Share of its time, at its end, during which a number fades out.
    static constexpr float FadeShare = 0.5f;

    DamageNumberSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs);

    void update(float elapsed) override;
};

} // namespace rtype::client
