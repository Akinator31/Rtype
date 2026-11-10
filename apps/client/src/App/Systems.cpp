#include "App/Systems.hpp"

#include "Campaign/TransmissionSystem.hpp"
#include "Console/ConsoleSystem.hpp"
#include "Controls/KeyCaptureSystem.hpp"
#include "Game/DamageNumberSystem.hpp"
#include "Game/DeathScreenSystem.hpp"
#include "Game/HealthBarSystem.hpp"
#include "Game/HudSystem.hpp"
#include "Game/ImpactSystem.hpp"
#include "Game/RescueCameraSystem.hpp"
#include "Game/ResultsSystem.hpp"
#include "Game/VoiceSystem.hpp"
#include "Lore/CrawlSystem.hpp"
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
    ecs.registerSystem<KeyCaptureSystem>(ecs, engine.getScenes(), devices);
    // After the menus, which read the input first: Escape opens the pause menu before the game reads it
    auto &sessions = ecs.registerSystem<GameSessionSystem>(ecs, engine.getScenes(), engine.getControls());
    // After the session, which moves the shogun it follows
    ecs.registerSystem<RescueCameraSystem>(sessions);
    // After the session, which gives the tick the voices wait for
    ecs.registerSystem<VoiceSystem>(sessions, audio);
    ecs.registerSystem<ImpactSystem>(ecs);
    ecs.registerSystem<DamageNumberSystem>(ecs);
    // After the session, which opens the death screens and gives the tick they count down to
    ecs.registerSystem<DeathScreenSystem>(ecs, sessions);
    ecs.registerSystem<HealthBarSystem>();
    // Before the sounds, which play those of the results in the frame they give them
    ecs.registerSystem<ResultsSystem>(ecs);
    // Before the sounds, which play its keys, and the anchors, which place its lines as they grow
    ecs.registerSystem<TransmissionSystem>(ecs, engine.getScenes(), engine.getControls(), devices);
    // After the session, which gives the players, and before the anchors, which place the HUD
    ecs.registerSystem<HudSystem>(sessions, metrics);
    // After the menus and the game, to play their sounds in the frame they give them
    ecs.registerSystem<engine::SoundSystem>(audio);
    ecs.registerSystem<engine::WidgetTextSystem>();
    ecs.registerSystem<engine::BoundTextSystem>();
    ecs.registerSystem<engine::TweenSystem>();
    ecs.registerSystem<engine::SpinSystem>();
    ecs.registerSystem<engine::ScrollSystem>();
    ecs.registerSystem<FlightSystem>();
    ecs.registerSystem<IntroSystem>(engine.getScenes(), devices);
    ecs.registerSystem<LogoFlareSystem>();
    ecs.registerSystem<CrawlSystem>(engine.getScenes());
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
