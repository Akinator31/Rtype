// Tests of the transitions between the main menu and its submenus: the flight of the camera to the view of
// the submenu that is open, and the fade of the interface of the scene that comes to the front.

#include "MainMenu/CameraFlight.hpp"
#include "MainMenu/Intro.hpp"
#include "MainMenu/MenuTransition.hpp"
#include "MainMenu/MenuTransitionSystem.hpp"
#include "MainMenu/Space.hpp"
#include "rtype/engine/Engine/Engine.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace rtype::client;
using Catch::Approx;
using rtype::ecs::entity_t;
using rtype::render::Camera;
using rtype::render::Vector3;

namespace
{

constexpr float frame = 1.0f / 60.0f;

/// How far two points are from each other.
float distance(Vector3 left, Vector3 right)
{
    return length(left - right);
}

/// A menu seen through a camera at the origin, with a button, and two scenes that open over it: its
/// "settings", which have a view ten units ahead, a label and a dimmed label, and "controls", which have
/// no view.
struct Menu {
    rtype::engine::Engine engine;
    Camera home{};
    Camera settingsView = lookAtView(home, {0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, -20.0f}, 0.0f);
    entity_t camera = 0;
    entity_t button = 0;
    entity_t planet = 0;
    entity_t label = 0;
    entity_t dimmed = 0;

    explicit Menu(bool introStarted = true)
    {
        rtype::ecs::Ecs &ecs = engine.getEcs();
        ecs.registerSystem<MenuTransitionSystem>(ecs, engine, engine.getScenes());
        engine.createScene("menu", {.updatesWhenCovered = true},
                           [this, introStarted](rtype::engine::Engine &, rtype::engine::SceneId scene) {
                               camera = spawn(scene, rtype::engine::Layer::World);
                               rtype::ecs::Ecs &world = engine.getEcs();
                               world.addComponent<rtype::engine::Camera>(camera, home);
                               world.addComponent<Intro>(camera, home, home, introStarted, 60.0f);
                               world.addComponent<MenuCameraFlight>(
                                   camera, std::vector<SubmenuView>{{"settings", settingsView}});
                               button = spawn(scene, rtype::engine::Layer::Interface);
                               planet = spawn(scene, rtype::engine::Layer::World);
                           });
        engine.createScene("settings", {.overlay = true, .hidesInterfaceBelow = true},
                           [this](rtype::engine::Engine &, rtype::engine::SceneId scene) {
                               label = spawn(scene, rtype::engine::Layer::Interface);
                               dimmed = spawn(scene, rtype::engine::Layer::Interface);
                               engine.getEcs().addComponent<rtype::engine::Opacity>(dimmed, 0.5f);
                           });
        engine.createScene("controls", {.overlay = true, .hidesInterfaceBelow = true});
        engine.switchScene("menu");
        run(frame);
    }

    entity_t spawn(rtype::engine::SceneId scene, rtype::engine::Layer layer)
    {
        rtype::ecs::Ecs &ecs = engine.getEcs();
        const entity_t entity = ecs.createEntity();
        ecs.addComponent<rtype::engine::SceneMember>(entity, scene);
        ecs.addComponent<rtype::engine::Drawable>(entity, layer);
        return entity;
    }

    void run(float seconds)
    {
        for (float time = 0.0f; time < seconds; time += frame) {
            engine.update(frame);
        }
    }

    [[nodiscard]] Vector3 position()
    {
        return engine.getEcs().getComponent<rtype::engine::Camera>(camera).view.position;
    }
    /// The opacity of an entity, 1 if it was never given one.
    [[nodiscard]] float opacity(entity_t entity)
    {
        rtype::ecs::Ecs &ecs = engine.getEcs();
        return ecs.hasComponent<rtype::engine::Opacity>(entity)
                   ? ecs.getComponent<rtype::engine::Opacity>(entity).value
                   : 1.0f;
    }
};

} // namespace

TEST_CASE("A view looks at a point of the world, or approaches it, and can keep it off the middle", "[menu]")
{
    const Camera view{};
    const Vector3 station{0.0f, 0.0f, -100.0f};

    const Camera from = lookAtView(view, {10.0f, 0.0f, 0.0f}, {}, 0.0f);
    CHECK(distance(from.position, {10.0f, 0.0f, 0.0f}) < 1e-4f);
    CHECK(distance(from.target - from.position, {-1.0f, 0.0f, 0.0f}) < 1e-4f);
    CHECK(from.fieldOfView == view.fieldOfView);

    // Along the line from the camera to the station, 30 away from it
    const Camera near = approachView(view, station, 30.0f, 0.0f);
    CHECK(distance(near.position, {0.0f, 0.0f, -70.0f}) < 1e-4f);
    CHECK(distance(near.target - near.position, {0.0f, 0.0f, -1.0f}) < 1e-4f);

    // Turned to the right, the view leaves the station on its left
    const Camera aside = approachView(view, station, 30.0f, 90.0f);
    CHECK(distance(aside.position, near.position) < 1e-4f);
    CHECK(distance(aside.target - aside.position, {1.0f, 0.0f, 0.0f}) < 1e-4f);
}

TEST_CASE("A flight goes from a view to the other, slowly at both ends", "[menu]")
{
    const Camera from{};
    const Camera to = lookAtView(from, {0.0f, 0.0f, -10.0f}, {5.0f, 0.0f, -10.0f}, 0.0f);

    CHECK(distance(mixViews(from, to, 0.0f).position, from.position) < 1e-4f);
    CHECK(distance(mixViews(from, to, 1.0f).position, to.position) < 1e-4f);
    CHECK(distance(mixViews(from, to, 1.0f).target, to.target) < 1e-4f);
    CHECK(mixViews(from, to, 0.5f).position.z == Approx(-5.0f));

    CHECK(easeInOut(0.0f) == 0.0f);
    CHECK(easeInOut(0.5f) == Approx(0.5f));
    CHECK(easeInOut(1.0f) == 1.0f);
    CHECK(easeInOut(0.1f) < 0.1f);
    CHECK(easeInOut(0.9f) > 0.9f);
    CHECK(easeInOut(5.0f) == 1.0f);
}

TEST_CASE("The camera flies to the view of the submenu that opens, and back when it closes", "[menu]")
{
    Menu menu;
    CHECK(distance(menu.position(), menu.home.position) < 1e-4f);

    menu.engine.pushScene("settings");
    menu.run(MenuFlightDuration / 2.0f);
    CHECK(menu.position().z < -1.0f);
    CHECK(menu.position().z > -9.0f);
    menu.run(MenuFlightDuration);
    CHECK(distance(menu.position(), menu.settingsView.position) < 1e-3f);

    menu.engine.popScene();
    menu.run(MenuFlightDuration + (2.0f * frame));
    CHECK(distance(menu.position(), menu.home.position) < 1e-3f);
}

TEST_CASE("A flight starts from where the camera is, without a jump", "[menu]")
{
    Menu menu;
    menu.engine.pushScene("settings");
    menu.run(MenuFlightDuration / 2.0f);
    const Vector3 turnedBack = menu.position();

    menu.engine.popScene();
    menu.run(2.0f * frame);

    CHECK(distance(menu.position(), turnedBack) < 0.1f);
}

TEST_CASE("A scene opened over a submenu keeps the view of the submenu", "[menu]")
{
    Menu menu;
    menu.engine.pushScene("settings");
    menu.run(MenuFlightDuration + (2.0f * frame));

    menu.engine.pushScene("controls");
    menu.run(MenuFlightDuration);

    CHECK(distance(menu.position(), menu.settingsView.position) < 1e-3f);
}

TEST_CASE("The interface of the scene that comes to the front fades in, to the opacity it had", "[menu]")
{
    Menu menu;
    menu.engine.pushScene("settings");
    menu.run(2.0f * frame);
    CHECK(menu.opacity(menu.label) < 0.5f);
    CHECK(menu.opacity(menu.dimmed) < 0.25f);

    menu.run(MenuFadeDuration + frame);
    CHECK(menu.opacity(menu.label) == 1.0f);
    CHECK(menu.opacity(menu.dimmed) == 0.5f);
    CHECK_FALSE(menu.engine.getEcs().hasComponent<MenuFade>(menu.label));

    // Back to the menu: its own interface fades in, not its world
    menu.engine.popScene();
    menu.run(2.0f * frame);
    CHECK(menu.opacity(menu.button) < 0.5f);
    CHECK(menu.opacity(menu.planet) == 1.0f);
    menu.run(MenuFadeDuration + frame);
    CHECK(menu.opacity(menu.button) == 1.0f);
}

TEST_CASE("Nothing fades before the intro of the menu has started", "[menu]")
{
    Menu menu(false);

    menu.engine.pushScene("settings");
    menu.run(2.0f * frame);

    CHECK(menu.opacity(menu.label) == 1.0f);
}
