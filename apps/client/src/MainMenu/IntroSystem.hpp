#pragma once

#include "MainMenu/Intro.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Layout.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/input/InputSource.hpp"

namespace rtype::client
{

/// Where the intro puts its camera now: in the asteroids until the player presses, then on its flight to
/// the view of the menu, where it stays.
[[nodiscard]] render::Camera computeIntroView(const Intro &intro);

/// Plays the intro of the main menu: waits until the player presses any key, gamepad button or mouse button
/// while the scene of the Intro receives the input, then flies its Camera from the asteroids to the view of
/// the menu, shows the Hidden elements with an IntroSlide and moves, scales and fades them all to their
/// place.
///
/// Register it before the AnchorSystem, which places the elements from their Anchor.
class IntroSystem : public ecs::System
{
    const engine::Scenes &_scenes;
    const input::InputSource &_input;
    ecs::ComponentCache<Intro> &_intros;
    ecs::ComponentCache<engine::Camera> &_cameras;
    ecs::ComponentCache<IntroSlide> &_slides;
    ecs::ComponentCache<engine::Anchor> &_anchors;
    ecs::ComponentCache<engine::Scale> &_scales;
    ecs::ComponentCache<engine::Opacity> &_opacities;
    ecs::ComponentCache<ecs::Hidden> &_hidden;
    /// Whether something was held at the previous update: holding a key does not start the intro, pressing
    /// it does.
    bool _wasInputHeld = true;

    [[nodiscard]] bool isAnyInputHeld() const;
    void start(Intro &intro);
    void placeCamera(ecs::entity_t entity, const Intro &intro);
    void placeSlides(const Intro &intro);

public:
    /// @param scenes, input They must outlive the system.
    IntroSystem(ecs::ComponentCaches &caches, const engine::Scenes &scenes, const input::InputSource &input);

    void update(float elapsed) override;
};

} // namespace rtype::client
