#include "MainMenu/MainMenu.hpp"

#include "Assets.hpp"
#include "MainMenu/Intro.hpp"
#include "MainMenu/Layout.hpp"
#include "MainMenu/LogoFlare.hpp"
#include "MainMenu/Planet.hpp"
#include "MainMenu/Ships.hpp"
#include "MainMenu/Station.hpp"
#include "MainMenu/Submenus.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"

#include <cmath>
#include <string>

namespace rtype::client
{

namespace
{

/// The buttons are in a column on the right of the screen, under the station, Play at the top and Quit at
/// the bottom: the centre of Quit is buttonsBottom above the bottom of the screen, and the centres of two
/// buttons are buttonsSpacing apart.
constexpr int buttonsCount = 4;
constexpr float buttonsBottom = 150.0f;
constexpr float buttonsSpacing = 76.0f;

/// The buttons fade in while they slide from this far on the right of their place, buttonsDelay seconds
/// after the player presses, while the camera flies; each button comes buttonsStagger seconds after the one
/// above it.
constexpr float buttonsEntrance = 60.0f;
constexpr float buttonsDelay = 1.0f;
constexpr float buttonsStagger = 0.1f;
constexpr float buttonsDuration = 0.8f;

/// The logo is a picture of 1920 x 1080 pixels, drawn at logoWidth game units while the camera waits in the
/// asteroids, in the middle of the screen. When the player presses, it rises by logoRise and shrinks to
/// logoMenuScale while the camera flies to the menu, to stay at the top of the screen.
constexpr render::Rectangle logoPicture{0.0f, 0.0f, 1920.0f, 1080.0f};
constexpr float logoWidth = 1500.0f;
constexpr float logoRise = -400.0f;
constexpr float logoMenuScale = 0.42f;
constexpr float logoDelay = 0.2f;
constexpr float logoDuration = 3.2f;

/// The flare is a picture of the same size as the logo, transparent except for a star, in flarePicture,
/// whose centre is on the top of the P. It is thrown from the left of the screen and falls on its place
/// along a parabola (see LogoFlareSystem). flareTrail copies follow it like the tail of a comet, each one
/// flareTrailLag seconds behind the one before, and fainter.
constexpr render::Rectangle flarePicture{1240.0f, 245.0f, 420.0f, 420.0f};
constexpr int flareTrail = 8;
constexpr float flareTrailLag = 0.035f;
constexpr float flareTrailFading = 0.75f;

/// Camera of the main menu: a narrow field of view, so that the planet on the side of the screen is not
/// distorted by the perspective.
render::Camera createMainMenuView()
{
    render::Camera view;
    view.position = {-18.2015f, 0.2817f, -21.8759f};
    view.target = {-17.8749f, 0.2868f, -22.8210f};
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
ecs::entity_t createMainMenuCamera(ecs::Ecs &ecs, engine::SceneId scene)
{
    const render::Camera view = createMainMenuView();
    const render::Camera asteroids = createAsteroidsView(view);
    const ecs::entity_t camera = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(camera, scene);
    ecs.addComponent<engine::Camera>(camera, asteroids);
    ecs.addComponent<engine::Light>(camera, createMainMenuSun());
    ecs.addComponent<Intro>(camera, asteroids, view);
    return camera;
}

/// The starry sky behind the planet: white stars on black, so that the planet and the ships stand out.
void createMainMenuSky(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources)
{
    const ecs::entity_t sky = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(sky, scene);
    ecs.addComponent<engine::Backdrop>(
        sky, resources.loadTexture(RTYPE_IMAGE_STARFIELD, render::TextureFiltering::Smooth));
}

/// The logo of the game, in the middle of the screen until the player presses, then at the top.
ecs::entity_t createLogo(ecs::Ecs &ecs, engine::InterfaceBuilder &interface,
                         render::ResourceManager &resources)
{
    const float height = logoWidth * logoPicture.height / logoPicture.width;
    const ecs::entity_t logo =
        interface.sprite(resources.loadTexture(RTYPE_IMAGE_LOGO, render::TextureFiltering::Smooth),
                         logoPicture, logoWidth, height, {engine::AnchorPoint::Center, 0.0f, 0.0f});
    ecs.addComponent<engine::Scale>(logo);
    ecs.addComponent<IntroSlide>(logo, IntroSlide{.from = {0.0f, 0.0f},
                                                  .to = {0.0f, logoRise},
                                                  .delay = logoDelay,
                                                  .duration = logoDuration,
                                                  .easing = engine::Easing::EaseOut,
                                                  .toScale = logoMenuScale});
    return logo;
}

/// The star of the flare picture, drawn over the logo: the flare itself, or one of its trail if `lag` is
/// not 0. The ones drawn last are on top.
void createLogoFlare(ecs::Ecs &ecs, engine::InterfaceBuilder &interface, render::TextureId texture,
                     LogoFlare flare, int order)
{
    const float size = flarePicture.width * logoWidth / logoPicture.width;
    const ecs::entity_t entity = interface.sprite(texture, flarePicture, size, size, {});
    ecs.getComponent<engine::Drawable>(entity).order += order;
    ecs.addComponent<engine::Scale>(entity, 0.0f);
    ecs.addComponent<engine::Opacity>(entity, 0.0f);
    ecs.addComponent<LogoFlare>(entity, flare);
}

/// The flare of the logo and its trail. It starts when the intro of `camera` starts, and lands on the
/// place it has in the full logo.
void createLogoFlares(ecs::Ecs &ecs, engine::InterfaceBuilder &interface, render::ResourceManager &resources,
                      ecs::entity_t logo, ecs::entity_t camera)
{
    const render::TextureId texture =
        resources.loadTexture(RTYPE_IMAGE_LOGO_FLARE, render::TextureFiltering::Smooth);
    const float unitsPerPixel = logoWidth / logoPicture.width;
    const render::Vector2 place{
        (flarePicture.x + (flarePicture.width / 2.0f) - (logoPicture.width / 2.0f)) * unitsPerPixel,
        (flarePicture.y + (flarePicture.height / 2.0f) - (logoPicture.height / 2.0f)) * unitsPerPixel};
    LogoFlare flare{.logo = logo, .intro = camera, .place = place};
    for (int copy = flareTrail; copy > 0; --copy) {
        flare.lag = static_cast<float>(copy) * flareTrailLag;
        flare.strength = std::pow(flareTrailFading, static_cast<float>(copy));
        createLogoFlare(ecs, interface, texture, flare, flareTrail - copy + 1);
    }
    flare.lag = 0.0f;
    flare.strength = 1.0f;
    createLogoFlare(ecs, interface, texture, flare, flareTrail + 1);
}

/// The text in capitals, like the buttons of the design: the letters of ASCII and the accented letters of
/// Latin-1 (é is written in UTF-8 as C3 A9, É as C3 89).
std::string toCapitals(std::string text)
{
    for (std::size_t index = 0; index < text.size(); ++index) {
        const auto byte = static_cast<unsigned char>(text[index]);
        if (byte >= 'a' && byte <= 'z') {
            text[index] = static_cast<char>(byte - ('a' - 'A'));
        } else if (byte == 0xC3 && index + 1 < text.size()) {
            const auto next = static_cast<unsigned char>(text[index + 1]);
            if (next >= 0xA0 && next <= 0xBE && next != 0xB7) {
                text[index + 1] = static_cast<char>(next - 0x20);
            }
            ++index;
        }
    }
    return text;
}

/// The button at `row` of the column, counted from the top, hidden until the player presses, then fading
/// in from the right.
void createButton(ecs::Ecs &ecs, engine::InterfaceBuilder &interface,
                  localization::Localization &translations, const std::string &key,
                  const std::function<void()> &onPress, int row)
{
    const auto rowsBelow = static_cast<float>(buttonsCount - 1 - row);
    const render::Vector2 place{-screenMargin, -buttonsBottom - (rowsBelow * buttonsSpacing)};
    const render::Vector2 entrance{place.x + buttonsEntrance, place.y};
    const float delay = buttonsDelay + (static_cast<float>(row) * buttonsStagger);
    const ecs::entity_t button =
        interface.button(toCapitals(translations.text(key)), onPress,
                              {engine::AnchorPoint::BottomRight, entrance.x, entrance.y});
    ecs.addComponent<engine::BoundText>(button,
                                        [&translations, key] { return toCapitals(translations.text(key)); });
    ecs.addComponent<engine::Opacity>(button);
    ecs.addComponent<ecs::Hidden>(button);
    ecs.addComponent<IntroSlide>(button, IntroSlide{.from = entrance,
                                                    .to = place,
                                                    .delay = delay,
                                                    .duration = buttonsDuration,
                                                    .easing = engine::Easing::EaseOut,
                                                    .fromOpacity = 0.0f});
}

/// The buttons of the menu: Play, Online and Settings open their submenu.
void createButtons(engine::Engine &engine, engine::InterfaceBuilder &interface,
                   localization::Localization &translations, const std::function<void()> &quit)
{
    ecs::Ecs &ecs = engine.getEcs();
    createButton(
        ecs, interface, translations, "menu.play", [&engine] { engine.pushScene(PlayMenuScene); }, 0);
    createButton(
        ecs, interface, translations, "menu.online", [&engine] { engine.pushScene(OnlineMenuScene); }, 1);
    createButton(
        ecs, interface, translations, "menu.settings", [&engine] { engine.pushScene(SettingsMenuScene); }, 2);
    createButton(ecs, interface, translations, "menu.quit", quit, 3);
}

} // namespace

void buildMainMenu(engine::Engine &engine, engine::SceneId scene, render::ResourceManager &resources,
                   const engine::InterfaceStyle &style, localization::Localization &translations,
                   const std::function<void()> &quit)
{
    ecs::Ecs &ecs = engine.getEcs();
    const ecs::entity_t camera = createMainMenuCamera(ecs, scene);
    createMainMenuSky(ecs, scene, resources);
    createPlanetSystem(ecs, scene, resources);
    createStation(ecs, scene, resources);
    createShips(ecs, scene, resources, {createMainMenuView(), getPlanetCentre(), getPlanetRadius()});

    engine::InterfaceBuilder interface(ecs, scene, style);
    const ecs::entity_t logo = createLogo(ecs, interface, resources);
    createLogoFlares(ecs, interface, resources, logo, camera);
    createButtons(engine, interface, translations, quit);
}

} // namespace rtype::client
