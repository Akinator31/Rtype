#pragma once

#include "Breaker.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Control/Controls.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/physics/Component/Motion.hpp"

namespace orbit
{

/// Moves the paddle with Left and Right of the controls, while its scene is played. The walls stop it (the
/// physics resolves the paddle against them).
class PaddleSystem : public rtype::ecs::System
{
    const rtype::engine::Scenes &_scenes;
    const rtype::engine::Controls &_controls;
    rtype::ecs::ComponentCache<Paddle> &_paddles;
    rtype::ecs::ComponentCache<rtype::physics::Velocity> &_velocities;

public:
    /// @param scenes, controls They must outlive the system.
    PaddleSystem(rtype::ecs::ComponentCaches &caches, const rtype::engine::Scenes &scenes,
                 const rtype::engine::Controls &controls);

    void update(float elapsed) override;
};

} // namespace orbit
