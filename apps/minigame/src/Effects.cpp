#include "Effects.hpp"

#include "Breaker.hpp"
#include "Visuals.hpp"
#include "rtype/physics/Component/Motion.hpp"

#include <cmath>
#include <numbers>

namespace orbit
{

namespace
{

namespace ecs = rtype::ecs;
namespace engine = rtype::engine;
using rtype::render::Color;

constexpr int sparkCount = 8;
constexpr float sparkSpeed = 420.0f;
constexpr float sparkSize = 26.0f;
constexpr float shockwaveSize = 160.0f;
constexpr float pointsRise = 90.0f;
/// Above the arena; the trail stays under the ball.
constexpr int effectsOrder = 20;
constexpr int trailOrder = 0;

/// An entity of the scene at this place that fades out.
ecs::entity_t spawnFading(ecs::Ecs &ecs, engine::SceneId scene, float x, float y, float duration,
                          engine::Layer layer = engine::Layer::World, int order = effectsOrder)
{
    const ecs::entity_t entity = spawnDrawn(ecs, scene, x, y, layer, order);
    ecs.addComponent<engine::Opacity>(entity);
    ecs.addComponent<Fade>(entity, duration, duration);
    return entity;
}

engine::Sprite pictureOf(const Picture &picture, float size, Color tint)
{
    return {picture.texture, picture.source, size, size, tint};
}

} // namespace

FadeSystem::FadeSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, const engine::Scenes &scenes)
    : _ecs(ecs), _scenes(scenes), _fades(use<Fade>(caches)), _opacities(use<engine::Opacity>(caches))
{
}

void FadeSystem::update(float elapsed)
{
    for (ecs::entity_t entity = 0; entity < _fades.size(); ++entity) {
        if (!_fades.has(entity) || !_scenes.updates(entity)) {
            continue;
        }
        Fade &fade = _fades.get(entity);
        fade.left -= elapsed;
        if (fade.left <= 0.0f) {
            _ecs.destroyEntity(entity);
        } else if (_opacities.has(entity)) {
            _opacities.get(entity).value = fade.left / fade.duration;
        }
    }
}

void spawnBurst(ecs::Ecs &ecs, engine::SceneId scene, const Looks &looks, float x, float y, Color color)
{
    const ecs::entity_t ring = spawnFading(ecs, scene, x, y, 0.35f);
    ecs.addComponent<engine::Sprite>(ring, pictureOf(looks.shockwave, shockwaveSize, color));
    popIn(ecs, ring, 0.2f, 0.35f);
    for (int index = 0; index < sparkCount; ++index) {
        const float angle = (2.0f * std::numbers::pi_v<float> * static_cast<float>(index)) / sparkCount;
        const ecs::entity_t spark = spawnFading(ecs, scene, x, y, 0.45f);
        ecs.addComponent<engine::Sprite>(spark, pictureOf(looks.spark, sparkSize, color));
        ecs.addComponent<rtype::physics::Velocity>(spark, std::cos(angle) * sparkSpeed,
                                                   std::sin(angle) * sparkSpeed);
    }
}

void spawnPoints(ecs::Ecs &ecs, engine::SceneId scene, const Looks &looks, float x, float y, int points)
{
    const ecs::entity_t text = spawnFading(ecs, scene, x, y, 0.8f);
    ecs.addComponent<engine::Text>(text, "+" + std::to_string(points), looks.font, 34.0f,
                                   Color{255, 240, 200, 255});
    ecs.addComponent<rtype::physics::Velocity>(text, 0.0f, -pointsRise);
}

void spawnBanner(ecs::Ecs &ecs, engine::SceneId scene, const Looks &looks, const std::string &text)
{
    const ecs::entity_t banner =
        spawnFading(ecs, scene, ArenaWidth / 2.0f, ArenaHeight * 0.55f, 1.8f, engine::Layer::Interface);
    ecs.addComponent<engine::Text>(banner, text, looks.font, 120.0f);
    ecs.addComponent<engine::Title>(banner);
    popIn(ecs, banner, 0.6f, 0.5f);
}

void spawnTrail(ecs::Ecs &ecs, engine::SceneId scene, float x, float y)
{
    const ecs::entity_t trail = spawnFading(ecs, scene, x, y, 0.22f, engine::Layer::World, trailOrder);
    ecs.addComponent<engine::Box>(trail, 14.0f, 14.0f, Color{120, 220, 255, 170});
}

} // namespace orbit
