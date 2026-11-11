#include "App/Systems.hpp"

#include "Console/ConsoleSystem.hpp"
#include "Controls/KeyCaptureSystem.hpp"
#include "Game/DamageNumberSystem.hpp"
#include "Game/DeathScreenSystem.hpp"
#include "Game/HealthBarSystem.hpp"
#include "Game/HudSystem.hpp"
#include "Game/ImpactSystem.hpp"
#include "Game/RescueCameraSystem.hpp"
#include "Game/ResultsSystem.hpp"
#include "Lore/CrawlSystem.hpp"
#include "MainMenu/FlightSystem.hpp"
#include "MainMenu/IntroSystem.hpp"
#include "MainMenu/LogoFlareSystem.hpp"
#include "MainMenu/MenuTransitionSystem.hpp"
#include "ShipSelector/ShipSelectorSystem.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Engine.hpp"

namespace rtype::client
{

ClientSystems registerClientSystems(engine::Engine &engine, input::InputSource &input,
                                    audio::AudioPlayer &audio, const engine::RendererMetrics &metrics,
                                    const engine::InterfaceStyle &style)
{
    ecs::Ecs &ecs = engine.getEcs();
    ecs.registerSystem<ConsoleSystem>(engine, input);
    const auto &interface =
        ecs.registerSystem<engine::InterfaceSystem>(ecs, engine.getScenes(), input, metrics, style);
    ecs.registerSystem<KeyCaptureSystem>(ecs, engine.getScenes(), input);
    // After the menus, which read the input first: Escape opens the pause menu before the game reads it
    auto &sessions = ecs.registerSystem<GameSessionSystem>(ecs, engine.getScenes(), engine.getControls());
    // After the session, which moves the shogun it follows
    ecs.registerSystem<RescueCameraSystem>(sessions);
    ecs.registerSystem<ImpactSystem>(ecs);
    ecs.registerSystem<DamageNumberSystem>(ecs);
    // After the session, which opens the death screens and gives the tick they count down to
    ecs.registerSystem<DeathScreenSystem>(ecs, sessions);
    ecs.registerSystem<HealthBarSystem>();
    // Before the sounds, which play those of the results in the frame they give them
    ecs.registerSystem<ResultsSystem>(ecs);
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
    ecs.registerSystem<IntroSystem>(engine.getScenes(), input);
    ecs.registerSystem<LogoFlareSystem>();
    // After the intro and the tweens: it has the last word on the camera and the opacities of the menus
    ecs.registerSystem<MenuTransitionSystem>(ecs, engine, engine.getScenes());
    ecs.registerSystem<CrawlSystem>(engine.getScenes());
    ecs.registerSystem<ShipSelectorSystem>(engine.getScenes(), engine.getControls(), input, metrics);
    ecs.registerSystem<engine::ModelAnimationSystem>();
    ecs.registerSystem<engine::HitboxSystem>(metrics, style);
    ecs.registerSystem<engine::ColumnLayoutSystem>(ecs);
    ecs.registerSystem<engine::AnchorSystem>(metrics);
    ecs.registerSystem<engine::ToastSystem>(ecs, metrics, style);
    ecs.registerSystem<ecs::TransformSystem>(ecs);
    return {.sessions = sessions, .interface = interface};
}

} // namespace rtype::client
