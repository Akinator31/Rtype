#include "App/Systems.hpp"

#include "Console/ConsoleSystem.hpp"
#include "Game/DamageNumberSystem.hpp"
#include "Game/HealthBarSystem.hpp"
#include "Game/ImpactSystem.hpp"
#include "MainMenu/FlightSystem.hpp"
#include "MainMenu/IntroSystem.hpp"
#include "MainMenu/LogoFlareSystem.hpp"
#include "ShipSelector/ShipSelectorSystem.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Engine.hpp"

namespace rtype::client
{

GameSessionSystem &registerClientSystems(engine::Engine &engine, input::Devices &devices,
                                         audio::AudioPlayer &audio, const engine::RendererMetrics &metrics,
                                         const engine::InterfaceStyle &style)
{
    ecs::Ecs &ecs = engine.getEcs();
    ecs.registerSystem<ConsoleSystem>(engine, devices);
    ecs.registerSystem<engine::InterfaceSystem>(ecs, engine.getScenes(), devices, metrics, style);
    // After the menus, which read the input first: Escape opens the pause menu before the game reads it
    auto &sessions = ecs.registerSystem<GameSessionSystem>(ecs, engine.getScenes(), engine.getControls());
    ecs.registerSystem<ImpactSystem>(ecs);
    ecs.registerSystem<DamageNumberSystem>(ecs);
    ecs.registerSystem<HealthBarSystem>();
    // After the menus and the game, to play their sounds in the frame they give them
    ecs.registerSystem<engine::SoundSystem>(audio);
    ecs.registerSystem<engine::WidgetTextSystem>();
    ecs.registerSystem<engine::BoundTextSystem>();
    ecs.registerSystem<engine::TweenSystem>();
    ecs.registerSystem<engine::SpinSystem>();
    ecs.registerSystem<FlightSystem>();
    ecs.registerSystem<IntroSystem>(engine.getScenes(), devices);
    ecs.registerSystem<LogoFlareSystem>();
    ecs.registerSystem<ShipSelectorSystem>(engine.getScenes(), engine.getControls(), devices);
    ecs.registerSystem<engine::ModelAnimationSystem>();
    ecs.registerSystem<engine::HitboxSystem>(metrics, style);
    ecs.registerSystem<engine::ColumnLayoutSystem>(ecs);
    ecs.registerSystem<engine::AnchorSystem>(metrics);
    ecs.registerSystem<engine::ToastSystem>(ecs, metrics, style);
    ecs.registerSystem<ecs::TransformSystem>(ecs);
    return sessions;
}

} // namespace rtype::client
