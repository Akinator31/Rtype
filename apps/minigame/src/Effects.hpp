#pragma once

#include "Looks.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Scene/Scenes.hpp"

#include <string>

namespace orbit
{

/// Fades the entity out over its duration, then destroys it: sparks, trails, floating points, banners.
struct Fade {
    float duration = 0.5f;
    float left = 0.5f;
};

/// Runs the Fades of the scenes that are played: their opacity goes down to 0, then they are destroyed.
class FadeSystem : public rtype::ecs::System
{
    rtype::ecs::Ecs &_ecs;
    const rtype::engine::Scenes &_scenes;
    rtype::ecs::ComponentCache<Fade> &_fades;
    rtype::ecs::ComponentCache<rtype::engine::Opacity> &_opacities;

public:
    /// @param ecs, scenes They must outlive the system.
    FadeSystem(rtype::ecs::ComponentCaches &caches, rtype::ecs::Ecs &ecs,
               const rtype::engine::Scenes &scenes);

    void update(float elapsed) override;
};

/// A brick breaking: a ring of light growing where it was, and sparks of its colour thrown around.
void spawnBurst(rtype::ecs::Ecs &ecs, rtype::engine::SceneId scene, const Looks &looks, float x, float y,
                rtype::render::Color color);
/// Points rising from where they were scored, and fading.
void spawnPoints(rtype::ecs::Ecs &ecs, rtype::engine::SceneId scene, const Looks &looks, float x, float y,
                 int points);
/// A big title in the middle of the arena that pops in and fades: "LEVEL 2".
void spawnBanner(rtype::ecs::Ecs &ecs, rtype::engine::SceneId scene, const Looks &looks,
                 const std::string &text);
/// A short-lived light left behind the ball.
void spawnTrail(rtype::ecs::Ecs &ecs, rtype::engine::SceneId scene, float x, float y);

} // namespace orbit
