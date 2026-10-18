#include "Game/GameScenes.hpp"

#include "Assets.hpp"
#include "Game/LocalSession.hpp"
#include "MainMenu/MainMenu.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"
#include "rtype/engine/Interface/MenuBuilder.hpp"
#include "rtype/game/Constants.hpp"

#include <format>
#include <string>
#include <utility>

namespace rtype::client
{

namespace
{

/// Distance of the HUD from the top left corner of the screen, and between two lines.
constexpr float HudMargin = 40.0f;
constexpr float HudLineSpacing = 56.0f;

} // namespace

GameScenes::GameScenes(engine::Engine &engine, render::ResourceManager &resources,
                       const engine::Metrics &metrics, const engine::InterfaceStyle &style,
                       GameSessionSystem &sessions)
    : _engine(engine), _sessions(sessions)
{
    engine.createScene(GameScene,
                       {.updatesWhenCovered = true, .onBack = [&engine] { engine.pushScene(PauseScene); }},
                       [this, &resources, &style](engine::Engine & /*target*/, engine::SceneId scene) {
                           build(scene, resources, style);
                       });
    engine.createScene(PauseScene, {.overlay = true},
                       [&metrics, &style](engine::Engine &target, engine::SceneId scene) {
                           engine::MenuBuilder menu(target.getEcs(), scene, metrics, style, "Pause", true);
                           menu.button("Resume", [&target] { target.popScene(); });
                           menu.button("Main menu", [&target] { target.switchScene(MainMenuScene); });
                       });
}

void GameScenes::play(std::unique_ptr<GameSession> session)
{
    _next = std::move(session);
    _engine.switchScene(GameScene);
}

void GameScenes::build(engine::SceneId scene, render::ResourceManager &resources,
                       const engine::InterfaceStyle &style)
{
    ecs::Ecs &ecs = _engine.getEcs();

    const ecs::entity_t sky = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(sky, scene);
    ecs.addComponent<engine::Backdrop>(
        sky, resources.loadTexture(RTYPE_IMAGE_STARFIELD, render::TextureFiltering::Smooth));

    // One line per player, in its colour, empty while the slot is free
    engine::InterfaceBuilder hud(ecs, scene, style);
    for (std::uint8_t slot = 0; slot < game::MaxPlayers; ++slot) {
        const ecs::entity_t line = hud.boundText(
            [this, slot] {
                for (const game::PlayerState &player : _sessions.players()) {
                    if (player.slot == slot) {
                        return std::format("P{}   Lives {}   Score {:06}", slot + 1, player.lives,
                                           player.score);
                    }
                }
                return std::string();
            },
            {engine::AnchorPoint::TopLeft, HudMargin,
             HudMargin + (static_cast<float>(slot) * HudLineSpacing)});
        ecs.getComponent<engine::Text>(line).color = playerColor(slot);
    }

    _sessions.start(_next ? std::move(_next) : std::make_unique<LocalSession>(), scene);
}

} // namespace rtype::client
