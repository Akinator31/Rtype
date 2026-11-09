#include "Systems.hpp"

#include "Effects.hpp"
#include "Hud.hpp"
#include "PaddleSystem.hpp"
#include "RulesSystem.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/System/AnchorSystem.hpp"
#include "rtype/engine/System/BoundTextSystem.hpp"
#include "rtype/engine/System/HitboxSystem.hpp"
#include "rtype/engine/System/InterfaceSystem.hpp"
#include "rtype/engine/System/ScrollSystem.hpp"
#include "rtype/engine/System/SoundSystem.hpp"
#include "rtype/engine/System/TweenSystem.hpp"
#include "rtype/engine/System/WidgetTextSystem.hpp"
#include "rtype/physics/Physics.hpp"

#include <utility>

namespace orbit
{

namespace engine = rtype::engine;
namespace physics = rtype::physics;

/// The cells of the broad phase of the collisions, about the size of a brick.
constexpr float collisionCellSize = 128.0f;

void registerGameSystems(engine::Engine &engine, rtype::audio::AudioPlayer &audio, Match &match,
                         BallSounds sounds, const Looks &looks, std::function<void()> onGameOver)
{
    rtype::ecs::Ecs &ecs = engine.getEcs();
    engine::Scenes &scenes = engine.getScenes();
    const auto played = [&scenes](rtype::ecs::entity_t entity) { return scenes.updates(entity); };
    ecs.registerSystem<PaddleSystem>(scenes, engine.getControls());
    ecs.registerSystem<physics::MovementSystem>(played);
    const auto &collisions = ecs.registerSystem<physics::CollisionSystem>(ecs, collisionCellSize, played);
    ecs.registerSystem<BallSystem>(ecs, scenes, engine.getControls(), collisions, match, audio, sounds,
                                   looks);
    ecs.registerSystem<RulesSystem>(ecs, scenes, match, looks, std::move(onGameOver));
    ecs.registerSystem<FadeSystem>(ecs, scenes);
    ecs.registerSystem<LifeIconSystem>(match);
    ecs.registerSystem<rtype::ecs::TransformSystem>(ecs);
}

void registerSystems(engine::Engine &engine, rtype::input::InputSource &input,
                     rtype::audio::AudioPlayer &audio, const engine::Metrics &metrics,
                     const engine::InterfaceStyle &style, Match &match, BallSounds sounds, const Looks &looks,
                     std::function<void()> onGameOver)
{
    rtype::ecs::Ecs &ecs = engine.getEcs();
    ecs.registerSystem<engine::InterfaceSystem>(ecs, engine.getScenes(), input, metrics, style);
    registerGameSystems(engine, audio, match, sounds, looks, std::move(onGameOver));
    ecs.registerSystem<engine::ScrollSystem>();
    ecs.registerSystem<engine::SoundSystem>(audio);
    ecs.registerSystem<engine::WidgetTextSystem>();
    ecs.registerSystem<engine::BoundTextSystem>();
    ecs.registerSystem<engine::TweenSystem>();
    // The places the mouse can click on the buttons, before the anchors place them
    ecs.registerSystem<engine::HitboxSystem>(metrics, style);
    ecs.registerSystem<engine::AnchorSystem>(metrics);
}

} // namespace orbit
