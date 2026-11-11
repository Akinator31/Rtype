#pragma once

#include "MainMenu/Intro.hpp"
#include "MainMenu/LogoFlare.hpp"
#include "MainMenu/MenuTransition.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Scene/Scenes.hpp"

#include <optional>

namespace rtype::client
{

/// Plays the transitions between the main menu and its submenus, once its intro has started:
/// - the camera with a MenuCameraFlight flies to the view of the submenu that is open, and back to the view
///   of the menu when it closes. A flight starts from where the camera is: the player can open and close a
///   submenu before the camera has arrived;
/// - when another scene comes to the front (a submenu opens, or closes and shows the one below again), its
///   interface fades in.
///
/// Register it after the IntroSystem, which places the camera of the menu and the opacities of its elements,
/// and after the TweenSystem: it has the last word on both while a transition plays.
class MenuTransitionSystem : public ecs::System
{
    const engine::Engine &_engine;
    const engine::Scenes &_scenes;
    ecs::Ecs &_ecs;
    ecs::ComponentCache<MenuCameraFlight> &_flights;
    ecs::ComponentCache<Intro> &_intros;
    ecs::ComponentCache<engine::Camera> &_cameras;
    ecs::ComponentCache<MenuFade> &_fades;
    ecs::ComponentCache<engine::Opacity> &_opacities;
    ecs::ComponentCache<engine::SceneMember> &_members;
    ecs::ComponentCache<engine::Drawable> &_drawables;
    ecs::ComponentCache<LogoFlare> &_flares;
    /// The scene at the front at the last update.
    std::optional<engine::SceneId> _top;

    [[nodiscard]] std::optional<std::size_t> findDestination(const MenuCameraFlight &flight) const;
    [[nodiscard]] bool hasIntroStarted() const;
    void fly(ecs::entity_t entity, MenuCameraFlight &flight, float elapsed);
    void fadeIn(engine::SceneId scene);
    void updateFades(float elapsed);

public:
    /// @param engine, ecs They must outlive the system.
    MenuTransitionSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, const engine::Engine &engine,
                         const engine::Scenes &scenes);

    void update(float elapsed) override;
};

} // namespace rtype::client
