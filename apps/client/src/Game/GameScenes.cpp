#include "Game/GameScenes.hpp"

#include "Assets.hpp"
#include "Game/EntityLook.hpp"
#include "Game/GameView.hpp"
#include "Game/LocalSession.hpp"
#include "MainMenu/MainMenu.hpp"
#include "ShipModel.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"
#include "rtype/engine/Interface/MenuBuilder.hpp"
#include "rtype/game/Constants.hpp"

#include <format>
#include <string>
#include <utility>
#include <vector>

namespace rtype::client
{

namespace
{

/// Distance of the HUD from the top left corner of the screen, and between two lines.
constexpr float HudMargin = 40.0f;
constexpr float HudLineSpacing = 56.0f;

/// The hulls of the ship models seen through the camera of the game, flames excluded, in model units:
/// measured on the models in their Move pose, the ship of the players facing right, the drop ship of the
/// Bydos facing left. The colliders of the ships have the same proportions (Rules::shipHalfWidth,
/// config/ships/bydo_scout.lua).
constexpr render::Rectangle EznoHull{-8.66f, -3.31f, 17.31f, 10.01f};
constexpr render::Rectangle DropShipHull{-11.95f, -5.74f, 23.9f, 9.11f};

/// The meshes of the flames and trails of the ship models, by index in their files.
const std::vector<int> EznoFlames{2, 6};
const std::vector<int> DropShipFlames{9, 10, 11, 12};

/// The cores of the shot models seen through the camera of the game, trails and rings excluded, in model
/// units, measured on the models: the shot of the players facing right, the shots of the Bydos facing left.
/// The colliders of the shots have the same proportions (config/projectiles/).
constexpr render::Rectangle ShotBasicCore{-1.6f, -0.55f, 4.0f, 1.1f};
constexpr render::Rectangle ShotSpiralCore{-2.4f, -0.73f, 4.48f, 1.46f};
constexpr render::Rectangle OrbPlasmaCore{-0.9f, -0.9f, 1.8f, 1.8f};

/// The ships are lit from the top left, in front of them, in the world of the game where y points down.
constexpr render::Light GameLight{{0.5f, 0.8f, 0.6f}, {255, 248, 240, 255}, {48, 50, 62, 255}};

/// The colours of the light around the shots, those of their energy; the alpha is how bright it is.
constexpr render::Color PlayerShotLight{80, 200, 255, 130};
constexpr render::Color BydoShotLight{185, 90, 255, 115};
constexpr render::Color PlasmaOrbLight{255, 120, 35, 135};
/// How far the light of a shot spreads around its core, in heights of the core.
constexpr float ShotLightSpread = 1.5f;

/// The energy of a shot with a light around its core.
/// @param core The core of the shot model, as in ShotLook.
render::Energy shine(render::Energy energy, render::TextureId light, render::Rectangle core,
                     render::Color color)
{
    const float margin = core.height * ShotLightSpread;
    // The cores are measured through the camera of the game, where y points down: the models have y up
    energy.halo = render::Energy::Halo{
        light,
        color,
        {core.x + (core.width / 2.0f), -(core.y + (core.height / 2.0f)), 0.0f},
        {core.width + (2.0f * margin), core.height + (2.0f * margin)},
    };
    return energy;
}

} // namespace

GameScenes::GameScenes(engine::Engine &engine, render::ResourceManager &resources,
                       const engine::Metrics &metrics, const engine::InterfaceStyle &style,
                       GameSessionSystem &sessions, localization::Localization &translations)
    : _engine(engine), _sessions(sessions), _translations(translations)
{
    sessions.setShipLook(game::EntityKind::PlayerShip,
                         {loadShipModel(resources, RTYPE_MODEL_SHIP_EZNO), EznoHull, false, EznoFlames});
    sessions.setShipLook(
        game::EntityKind::BydoFlyer,
        {loadShipModel(resources, RTYPE_MODEL_SHIP_DROP_SHIP), DropShipHull, true, DropShipFlames});
    const render::Energy energy{
        resources.loadTexture(RTYPE_IMAGE_ENERGY_NOISE, render::TextureFiltering::Smooth)};
    const render::TextureId light =
        resources.loadTexture(RTYPE_IMAGE_GLOW_CORE, render::TextureFiltering::Smooth);
    sessions.setShotLook(game::EntityKind::PlayerShot,
                         {resources.loadModel(RTYPE_MODEL_SHOT_BASIC), ShotBasicCore, false,
                          shine(energy, light, ShotBasicCore, PlayerShotLight)});
    sessions.setShotLook(game::EntityKind::BydoShot,
                         {resources.loadModel(RTYPE_MODEL_SHOT_SPIRAL), ShotSpiralCore, true,
                          shine(energy, light, ShotSpiralCore, BydoShotLight)});
    sessions.setShotLook(game::EntityKind::BydoPlasmaShot,
                         {resources.loadModel(RTYPE_MODEL_ORB_PLASMA), OrbPlasmaCore, true,
                          shine(energy, light, OrbPlasmaCore, PlasmaOrbLight)});
    engine.createScene(GameScene,
                       {.updatesWhenCovered = true, .onBack = [&engine] { engine.pushScene(PauseScene); }},
                       [this, &resources, &metrics, &style](engine::Engine & /*target*/,
                                                            engine::SceneId scene) {
                           build(scene, resources, metrics, style);
                       });
    engine.createScene(PauseScene, {.overlay = true},
                       [this, &metrics, &style](engine::Engine &target, engine::SceneId scene) {
                           engine::MenuBuilder menu(target.getEcs(), scene, metrics, style,
                                                    _translations.text("game.pause"), true);
                           menu.button(_translations.text("game.resume"),
                                       [&target] { target.popScene(); });
                           menu.button(_translations.text("game.leave"), [this] { end(); });
                       });
}

void GameScenes::play(std::unique_ptr<GameSession> session, std::function<void()> onEnd)
{
    _next = std::move(session);
    _nextEnd = std::move(onEnd);
    _engine.switchScene(GameScene);
}

void GameScenes::end()
{
    _sessions.stop();
    const std::function<void()> onEnd = std::exchange(_onEnd, nullptr);
    if (onEnd) {
        onEnd();
    } else {
        _engine.switchScene(MainMenuScene);
    }
}

void GameScenes::build(engine::SceneId scene, render::ResourceManager &resources,
                       const engine::Metrics &metrics, const engine::InterfaceStyle &style)
{
    ecs::Ecs &ecs = _engine.getEcs();

    const ecs::entity_t sky = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(sky, scene);
    ecs.addComponent<engine::Backdrop>(
        sky, resources.loadTexture(RTYPE_IMAGE_STARFIELD, render::TextureFiltering::Smooth));
    ecs.addComponent<engine::Camera>(sky, computeGameView(metrics.visibleArea()));
    ecs.addComponent<engine::Light>(sky, GameLight);

    // One line per player, in its colour, empty while the slot is free
    engine::InterfaceBuilder hud(ecs, scene, style);
    for (std::uint8_t slot = 0; slot < game::MaxPlayers; ++slot) {
        const ecs::entity_t line = hud.boundText(
            [this, slot] {
                for (const game::PlayerState &player : _sessions.players()) {
                    if (player.slot == slot) {
                        return std::format("P{}   {} {}   {} {:06}", slot + 1,
                                           _translations.text("hud.lives"), player.lives,
                                           _translations.text("hud.score"), player.score);
                    }
                }
                return std::string();
            },
            {engine::AnchorPoint::TopLeft, HudMargin,
             HudMargin + (static_cast<float>(slot) * HudLineSpacing)});
        ecs.getComponent<engine::Text>(line).color = playerColor(slot);
    }

    _onEnd = std::exchange(_nextEnd, nullptr);
    _sessions.start(_next ? std::move(_next) : std::make_unique<LocalSession>(), scene, [this] { end(); });
}

} // namespace rtype::client
