#include "MainMenu/MainMenu.hpp"

#include "Assets.hpp"
#include "MainMenu/Intro.hpp"
#include "MainMenu/Layout.hpp"
#include "MainMenu/Planet.hpp"
#include "MainMenu/Ships.hpp"
#include "MainMenu/Station.hpp"
#include "MainMenu/Submenus.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"

#include <string>

namespace rtype::client
{

namespace
{

// Distance between the tops of two buttons, one above the other, as in the design
constexpr float buttonsSpacing = 103.0f;

/// The buttons are in a column in the bottom left corner, Play at the top and Quit at the bottom.
constexpr int buttonsCount = 4;

/// The buttons come from this far beyond their place, out of the left edge of the screen, buttonsDelay
/// seconds after the player presses; each button comes buttonsStagger seconds after the one above it.
constexpr float buttonsEntrance = 800.0f;
constexpr float buttonsDelay = 0.4f;
constexpr float buttonsStagger = 0.08f;

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

/// The button at `row` of the column, counted from the top, hidden until the player presses, then coming
/// from the left edge.
void createButton(ecs::Ecs &ecs, engine::InterfaceBuilder &interface, const std::string &text,
                  const std::function<void()> &onPress, int row)
{
    const auto rowsBelow = static_cast<float>(buttonsCount - 1 - row);
    const render::Vector2 place{screenMargin, -screenMargin - (rowsBelow * buttonsSpacing)};
    const render::Vector2 entrance{place.x - buttonsEntrance, place.y};
    const float delay = buttonsDelay + (static_cast<float>(row) * buttonsStagger);
    const ecs::entity_t button =
        interface.button(text, onPress, {engine::AnchorPoint::BottomLeft, entrance.x, entrance.y});
    ecs.addComponent<ecs::Hidden>(button);
    ecs.addComponent<IntroSlide>(button, entrance, place, delay, engine::Easing::EaseOutBack);
}

/// The buttons of the menu: Play, Online and Settings open their submenu.
void createButtons(engine::Engine &engine, engine::InterfaceBuilder &interface,
                   const std::function<void()> &quit)
{
    ecs::Ecs &ecs = engine.getEcs();
    createButton(ecs, interface, "Play", [&engine] { engine.pushScene(PlayMenuScene); }, 0);
    createButton(ecs, interface, "Online", [&engine] { engine.pushScene(OnlineMenuScene); }, 1);
    createButton(ecs, interface, "Settings", [&engine] { engine.pushScene(SettingsMenuScene); }, 2);
    createButton(ecs, interface, "Quit", quit, 3);
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
    createButtons(engine, interface, quit);
}

} // namespace rtype::client
