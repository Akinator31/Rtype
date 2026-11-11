#pragma once

#include "Breaker.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"

namespace orbit
{

/// The bar along the top of the arena: the score and the combo on the left, the level in the middle, the
/// lives on the right; and, while the ball waits on the paddle, how to launch it.
void buildHud(rtype::engine::InterfaceBuilder &interface, rtype::ecs::Ecs &ecs, rtype::engine::SceneId scene,
              const Match &match);

/// Lights the LifeIcons of the lives the player still has, and dims the lost ones.
class LifeIconSystem : public rtype::ecs::System
{
    const Match &_match;
    rtype::ecs::ComponentCache<LifeIcon> &_icons;
    rtype::ecs::ComponentCache<rtype::engine::Opacity> &_opacities;

public:
    /// @param match It must outlive the system.
    LifeIconSystem(rtype::ecs::ComponentCaches &caches, const Match &match);

    void update(float elapsed) override;
};

} // namespace orbit
