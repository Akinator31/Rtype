#include "MainMenu/MainMenu.hpp"

#include "Assets.hpp"

#include "MainMenu/Planet.hpp"
#include "MainMenu/Ships.hpp"
#include "MainMenu/Station.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"

namespace rtype::client
{

namespace
{

// Distance between the buttons and the edges of the screen, as on the left side of the design
constexpr float screenMargin = 84.0f;

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

/// Creates the entity that the main menu is seen through and lit by.
void createMainMenuCamera(ecs::Ecs &ecs, engine::SceneId scene)
{
    const ecs::entity_t camera = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(camera, scene);
    ecs.addComponent<engine::Camera>(camera, createMainMenuView());
    ecs.addComponent<engine::Light>(camera, createMainMenuSun());
}

/// The starry sky behind the planet: white stars on black, so that the planet and the ships stand out.
void createMainMenuSky(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources)
{
    const ecs::entity_t sky = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(sky, scene);
    ecs.addComponent<engine::Backdrop>(sky, resources.loadTexture(RTYPE_IMAGE_STARFIELD));
}

} // namespace

void buildMainMenu(engine::Engine &engine, engine::SceneId scene, render::ResourceManager &resources,
                   const engine::InterfaceStyle &style, const std::function<void()> &quit)
{
    createMainMenuCamera(engine.getEcs(), scene);
    createMainMenuSky(engine.getEcs(), scene, resources);
    createPlanetSystem(engine.getEcs(), scene, resources);
    createStation(engine.getEcs(), scene, resources);
    createShips(engine.getEcs(), scene, resources);

    engine::InterfaceBuilder buttons(engine.getEcs(), scene, style);
    buttons.button("Quit", quit, {engine::AnchorPoint::BottomRight, -screenMargin, -screenMargin});
}

} // namespace rtype::client
