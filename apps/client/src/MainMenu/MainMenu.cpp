#include "MainMenu/MainMenu.hpp"

#include "Assets.hpp"
#include "MainMenu/Intro.hpp"
#include "MainMenu/Planet.hpp"
#include "MainMenu/Ships.hpp"
#include "MainMenu/Station.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"

#include <string>

namespace rtype::client
{

namespace
{

// Distance between the buttons and the edges of the screen, as on the left side of the design
constexpr float screenMargin = 84.0f;

/// The buttons come from this far beyond their place, out of the right edge of the screen, buttonsDelay
/// seconds after the player presses.
constexpr float buttonsEntrance = 800.0f;
constexpr float buttonsDelay = 0.4f;

/// Camera of the main menu: a narrow field of view, so that the planet on the side of the screen is not
/// distorted by the perspective.
render::Camera createMainMenuView()
{
    render::Camera view;
    view.fieldOfView = 30.0f;
    return view;
}

/// The sun is on the right, a little behind the planet and above: the side facing the menu is lit, the night
/// side fades out towards the edge of the screen.
render::Light createMainMenuSun()
{
    render::Light sun;
    sun.direction = {-1.0f, -0.25f, 0.45f};
    sun.color = {255, 244, 225, 255};
    sun.ambient = {10, 10, 16, 255};
    return sun;
}

/// Creates the entity that the main menu is seen through and lit by. It starts in the asteroids, and flies
/// to the view of the menu when the player presses.
void createMainMenuCamera(ecs::Ecs &ecs, engine::SceneId scene)
{
    const render::Camera view = createMainMenuView();
    const render::Camera asteroids = createAsteroidsView(view);
    const ecs::entity_t camera = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(camera, scene);
    ecs.addComponent<engine::Camera>(camera, asteroids);
    ecs.addComponent<engine::Light>(camera, createMainMenuSun());
    ecs.addComponent<Intro>(camera, asteroids, view);
}

/// The starry sky behind the planet: white stars on black, so that the planet and the ships stand out.
void createMainMenuSky(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources)
{
    const ecs::entity_t sky = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(sky, scene);
    ecs.addComponent<engine::Backdrop>(
        sky, resources.loadTexture(RTYPE_IMAGE_STARFIELD, render::TextureFiltering::Smooth));
}

/// A button in the bottom right corner, hidden until the player presses, then coming from the right edge.
void createButton(ecs::Ecs &ecs, engine::InterfaceBuilder &interface, const std::string &text,
                  const std::function<void()> &onPress)
{
    const render::Vector2 place{-screenMargin, -screenMargin};
    const render::Vector2 entrance{place.x + buttonsEntrance, place.y};
    const ecs::entity_t button =
        interface.button(text, onPress, {engine::AnchorPoint::BottomRight, entrance.x, entrance.y});
    ecs.addComponent<ecs::Hidden>(button);
    ecs.addComponent<IntroSlide>(button, entrance, place, buttonsDelay, engine::Easing::EaseOutBack);
}

} // namespace

void buildMainMenu(engine::Engine &engine, engine::SceneId scene, render::ResourceManager &resources,
                   const engine::InterfaceStyle &style, const std::function<void()> &quit)
{
    ecs::Ecs &ecs = engine.getEcs();
    createMainMenuCamera(ecs, scene);
    createMainMenuSky(ecs, scene, resources);
    createPlanetSystem(ecs, scene, resources);
    createStation(ecs, scene, resources);
    createShips(ecs, scene, resources);

    engine::InterfaceBuilder interface(ecs, scene, style);
    createButton(ecs, interface, "Quit", quit);
}

} // namespace rtype::client
