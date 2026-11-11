#include "ShipSelector/ShipSelector.hpp"

#include "Assets.hpp"
#include "MainMenu/Layout.hpp"
#include "ShipModel.hpp"
#include "ShipSelector/ShipCarousel.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"

#include <cstddef>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace rtype::client
{

namespace
{

/// The pedestal is at the origin, its top at about 0.4 units, narrower than the ships; the ship floats over
/// it, and the player turns it with the mouse (ShipSelectorSystem).
constexpr float pedestalScale = 0.5f;
constexpr render::Vector3 shipPosition{0.0f, 1.15f, 0.0f};

/// The ships are seen from three quarters at rest: turned from the side towards the camera, in degrees.
constexpr float restingYaw = -32.0f;

/// The camera looks at the pedestal from a little above and far enough to see the ships whole, the pedestal
/// in the lower half of the screen, over the name of the ship.
render::Camera createHangarView()
{
    render::Camera view;
    view.position = {0.0f, 3.6f, 10.0f};
    view.target = {0.0f, 0.9f, 0.0f};
    view.fieldOfView = 30.0f;
    return view;
}

/// Light from the front and above, with an edge of cold light from behind: the hull stands out from the sky.
render::Light createHangarLight()
{
    render::Light light;
    light.direction = {-0.35f, -0.65f, -0.65f};
    light.color = {255, 250, 240, 255};
    light.ambient = {34, 38, 52, 255};
    light.rimDirection = {0.6f, -0.4f, 1.0f};
    light.rim = {120, 160, 235, 255};
    return light;
}

/// Where the interface is placed, from the edges of the screen, in game units.
constexpr float arrowsHeight = -150.0f;
constexpr float equipBottom = -270.0f;
constexpr float nameBottom = -200.0f;
constexpr float nameSize = 46.0f;
constexpr float dotsBottom = -170.0f;
constexpr float dotsSpacing = 70.0f;
constexpr float dotHeight = 3.0f;
constexpr float gaugesBottom = -80.0f;
constexpr float gaugesSpacing = 600.0f;
/// The rows of the two weapons, one under the other in the top right corner, over the sky.
constexpr float weaponsWidth = 640.0f;
constexpr float weaponsGap = 12.0f;

void createHangarCamera(ecs::Ecs &ecs, engine::SceneId scene)
{
    const ecs::entity_t camera = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(camera, scene);
    ecs.addComponent<engine::Camera>(camera, createHangarView());
    ecs.addComponent<engine::Light>(camera, createHangarLight());
}

void createHangarSky(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources)
{
    const ecs::entity_t sky = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(sky, scene);
    ecs.addComponent<engine::Backdrop>(
        sky, resources.loadTexture(RTYPE_IMAGE_STARFIELD, render::TextureFiltering::Smooth));
}

void createPedestal(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources)
{
    const ecs::entity_t pedestal = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(pedestal, scene);
    ecs.addComponent<engine::Drawable>(pedestal, engine::Layer::World, 0);
    ecs.addComponent<engine::Model>(pedestal, resources.loadModel(RTYPE_MODEL_PEDESTAL),
                                    render::Vector3{0.0f, 0.0f, 0.0f}, render::Vector3{0.0f, 1.0f, 0.0f},
                                    0.0f, pedestalScale);
}

/// The ship of the catalog at `index`, over the pedestal or on a side of it: the ShipSelectorSystem places
/// it.
ecs::entity_t createShip(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources,
                         std::size_t index)
{
    const ShipEntry &entry = getShipCatalog()[index];
    const ShipModel ship = loadShipModel(resources, entry.modelPath);
    const ecs::entity_t entity = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(entity, scene);
    ecs.addComponent<engine::Drawable>(entity, engine::Layer::World, 1);
    ecs.addComponent<engine::Model>(entity, ship.model, shipPosition, render::Vector3{0.0f, 1.0f, 0.0f}, 0.0f,
                                    entry.modelScale);
    ecs.addComponent<engine::ModelAnimation>(entity, ship.animations.idle);
    return entity;
}

/// The carousel that runs the ships, which start at rest on the one looked at, a little turned so that their
/// nose comes towards the player.
ecs::entity_t createCarousel(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources,
                             ShipSelection &selection)
{
    selection.slide = static_cast<float>(selection.focused);
    const ecs::entity_t entity = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(entity, scene);
    ShipCarousel &carousel = ecs.addComponent<ShipCarousel>(entity);
    carousel.selection = &selection;
    for (std::size_t index = 0; index < shipCount; ++index) {
        carousel.ships.push_back(createShip(ecs, scene, resources, index));
        carousel.yaws.push_back(restingYaw);
    }
    return entity;
}

/// The arrows on each side of the pedestal, to look at the previous and the next ship.
void createArrows(engine::InterfaceBuilder &interface, ShipSelection &selection)
{
    interface.arrowButton("<", [&selection] { selection.step(-1); },
                          {engine::AnchorPoint::Left, screenMargin, arrowsHeight});
    interface.arrowButton(">", [&selection] { selection.step(1); },
                          {engine::AnchorPoint::Right, -screenMargin, arrowsHeight});
}

/// The code and the name of the ship looked at, in the title style, under the pedestal.
ecs::entity_t createShipName(ecs::Ecs &ecs, engine::InterfaceBuilder &interface,
                             localization::Localization &translations, const ShipSelection &selection)
{
    const ecs::entity_t name = interface.boundTitle(
        [&translations, &selection] {
            const ShipEntry &ship = selection.getShown();
            return std::string(ship.code) + " " + translations.text(ship.nameKey);
        },
        {engine::AnchorPoint::Bottom, 0.0f, nameBottom});
    ecs.getComponent<engine::Text>(name).size = nameSize;
    ecs.addComponent<engine::Opacity>(name);
    return name;
}

/// What the button under the name says: equip the ship, or that it is equipped, or locked and the level it
/// needs; launch the game with it before a game.
std::string getEquipText(localization::Localization &translations, const ShipSelection &selection)
{
    const std::size_t shown = selection.getShownIndex();
    if (selection.isLocked(shown)) {
        return std::format("{} - {} {}", translations.text("ship.locked"),
                           translations.text("progress.level"), selection.requiredLevels[shown]);
    }
    if (selection.launch) {
        return translations.text("ship.launch");
    }
    const bool equipped = shown == selection.equipped;
    return translations.text(equipped ? "ship.equipped" : "ship.equip");
}

/// Equips the ship looked at and keeps it in the settings; before a game, closes the hangar and launches it.
void equip(engine::Engine &target, ShipSelection &selection)
{
    if (selection.isLocked(selection.focused)) {
        return;
    }
    selection.equipFocused();
    writeEquippedShip(selection, target.getSettings());
    if (selection.launch) {
        const std::function<void()> launch = std::exchange(selection.launch, nullptr);
        target.popScene();
        launch();
    }
}

ecs::entity_t createEquipButton(engine::Engine &target, engine::InterfaceBuilder &interface,
                                localization::Localization &translations, ShipSelection &selection)
{
    ecs::Ecs &ecs = target.getEcs();
    const ecs::entity_t button = interface.button(getEquipText(translations, selection),
                                                  [&target, &selection] { equip(target, selection); },
                                                  {engine::AnchorPoint::Bottom, 0.0f, equipBottom});
    ecs.addComponent<engine::BoundText>(
        button, [&translations, &selection] { return getEquipText(translations, selection); });
    ecs.addComponent<engine::Opacity>(button);
    return button;
}

/// One dot per ship under its name; the ShipSelectorSystem lights the one of the ship looked at.
std::vector<ecs::entity_t> createDots(engine::InterfaceBuilder &interface)
{
    std::vector<ecs::entity_t> dots;
    const float first = -static_cast<float>(shipCount - 1) / 2.0f;
    for (std::size_t index = 0; index < shipCount; ++index) {
        const float x = (first + static_cast<float>(index)) * dotsSpacing;
        dots.push_back(interface.box(0.0f, dotHeight, render::colors::White,
                                     {engine::AnchorPoint::Bottom, x, dotsBottom}));
    }
    return dots;
}

/// A row of the loadout: the weapon at `place`, changed with Left, Right and a click, and kept in the
/// settings at once.
void createWeaponRow(engine::Engine &target, engine::InterfaceBuilder &interface,
                     localization::Localization &translations, ShipSelection &selection, std::size_t place,
                     float top)
{
    const char *caption = place == 0 ? "ship.weapon_first" : "ship.weapon_second";
    const ecs::entity_t row = interface.settingRow(
        translations.text(caption),
        [&translations, &selection, place] {
            return translations.text("weapon." + selection.weapons[selection.loadout[place]].weapon);
        },
        [&target, &selection, place](int direction) {
            selection.stepWeapon(place, direction);
            writeEquippedWeapons(selection, target.getSettings());
        },
        {engine::AnchorPoint::TopRight, -screenMargin, top}, weaponsWidth);
    target.getEcs().addComponent<engine::BoundText>(
        row, [&translations, caption] { return translations.text(caption); });
}

/// The two weapons taken into the games, when the hangar has two open.
void createWeaponRows(engine::Engine &target, engine::InterfaceBuilder &interface,
                      const engine::InterfaceStyle &style, localization::Localization &translations,
                      ShipSelection &selection)
{
    if (selection.getLoadout()[0].empty()) {
        return;
    }
    for (std::size_t place = 0; place < game::LoadoutSize; ++place) {
        const float top = screenMargin + (static_cast<float>(place) * (style.setting.height + weaponsGap));
        createWeaponRow(target, interface, translations, selection, place, top);
    }
}

/// The characteristics of the ship looked at, as gauges, side by side along the bottom.
void createStatGauges(engine::InterfaceBuilder &interface, localization::Localization &translations,
                      const ShipSelection &selection)
{
    interface.gauge(translations.text("ship.speed"),
                    [&selection] { return selection.getBlend(&game::ShipStats::speed); },
                    {engine::AnchorPoint::Bottom, -gaugesSpacing, gaugesBottom});
    interface.gauge(translations.text("ship.power"),
                    [&selection] { return selection.getBlend(&game::ShipStats::power); },
                    {engine::AnchorPoint::Bottom, 0.0f, gaugesBottom});
    interface.gauge(translations.text("ship.armor"),
                    [&selection] { return selection.getBlend(&game::ShipStats::armor); },
                    {engine::AnchorPoint::Bottom, gaugesSpacing, gaugesBottom});
}

} // namespace

void createShipSelectorScene(engine::Engine &engine, render::ResourceManager &resources,
                             const engine::InterfaceStyle &style, localization::Localization &translations,
                             ShipSelection &selection)
{
    engine.createScene(
        ShipSelectorScene, {},
        [&resources, &style, &translations, &selection](engine::Engine &target, engine::SceneId scene) {
            ecs::Ecs &ecs = target.getEcs();
            selection.focused = selection.equipped;
            createHangarCamera(ecs, scene);
            createHangarSky(ecs, scene, resources);
            createPedestal(ecs, scene, resources);
            const ecs::entity_t carouselEntity = createCarousel(ecs, scene, resources, selection);

            engine::InterfaceBuilder interface(ecs, scene, style);
            interface.boundHeader([&translations] { return translations.text("ship.title"); },
                                  [&target] { target.popScene(); },
                                  {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});
            createArrows(interface, selection);
            const ecs::entity_t name = createShipName(ecs, interface, translations, selection);
            const ecs::entity_t equip = createEquipButton(target, interface, translations, selection);
            ShipCarousel &carousel = ecs.getComponent<ShipCarousel>(carouselEntity);
            carousel.fading = {name, equip};
            carousel.dots = createDots(interface);
            createStatGauges(interface, translations, selection);
            createWeaponRows(target, interface, style, translations, selection);
        });
}

void openShipSelector(engine::Engine &engine, ShipSelection &selection, std::function<void()> launch,
                      bool allUnlocked)
{
    selection.setAllUnlocked(allUnlocked);
    selection.launch = std::move(launch);
    engine.pushScene(ShipSelectorScene);
}

} // namespace rtype::client
