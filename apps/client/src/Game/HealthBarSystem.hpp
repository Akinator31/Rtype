#pragma once

#include "Game/HealthBarLooks.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"

namespace rtype::client
{

/// Shows the health bars: slides each one down to the hit points left, shows it for a while after the last
/// hit, then fades it out until the next one.
class HealthBarSystem : public ecs::System
{
    ecs::ComponentCache<HealthBar> &_bars;
    ecs::ComponentCache<engine::Opacity> &_opacities;

public:
    /// How long a bar shows after a hit, then how long it takes to fade out, in seconds.
    static constexpr float ShowDuration = 2.0f;
    static constexpr float FadeDuration = 0.4f;
    /// How fast a bar goes down to the hit points left, in shares of the bar per second.
    static constexpr float DrainSpeed = 1.5f;

    explicit HealthBarSystem(ecs::ComponentCaches &caches);

    void update(float elapsed) override;
};

} // namespace rtype::client
