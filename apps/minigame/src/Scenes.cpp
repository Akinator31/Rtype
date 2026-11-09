#include "Scenes.hpp"

#include "Arena.hpp"
#include "Effects.hpp"
#include "Hud.hpp"
#include "Levels.hpp"
#include "Visuals.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"
#include "rtype/log/Logger.hpp"

#include <exception>
#include <string>
#include <utility>

namespace orbit
{

namespace
{

namespace ecs = rtype::ecs;
namespace engine = rtype::engine;
using engine::AnchorPoint;
using rtype::render::Color;

/// Space between the buttons of a menu.
constexpr float buttonSpacing = 100.0f;
constexpr Color subtitleColor{150, 180, 220, 255};
constexpr Color dimColor{2, 4, 12, 190};
/// The veil of an overlay, under its texts.
constexpr int dimOrder = -10;

/// A row of bricks of every colour across the menu, each one shimmering at its own pace.
void spawnShimmeringRow(ecs::Ecs &ecs, engine::SceneId scene, float y)
{
    const float step = ArenaWidth / BrickColumns;
    for (int column = 0; column < BrickColumns; ++column) {
        const int strength = 1 + (column % MaxBrickStrength);
        const ecs::entity_t brick = spawnDrawn(ecs, scene, (static_cast<float>(column) + 0.5f) * step, y);
        ecs.addComponent<engine::Box>(brick, engine::Box{.width = BrickWidth,
                                                         .height = BrickHeight,
                                                         .color = brickColor(strength),
                                                         .bottom = brickBottom(strength)});
        const float duration = 0.8f + (0.15f * static_cast<float>((column * 7) % 9));
        ecs.addComponent<engine::Tween>(brick, pingPong(engine::TweenTarget::Opacity, 1.0f, 0.35f, duration));
    }
}

void buildMenu(engine::Engine &target, engine::SceneId scene, const engine::InterfaceStyle &style,
               const Looks &looks, const Match &match, const std::function<void()> &quit)
{
    ecs::Ecs &ecs = target.getEcs();
    spawnSky(ecs, scene, looks);
    spawnShimmeringRow(ecs, scene, 120.0f);
    spawnShimmeringRow(ecs, scene, ArenaHeight - 120.0f);
    engine::InterfaceBuilder interface(ecs, scene, style);
    const ecs::entity_t title = interface.title("ORBIT BREAKER", {AnchorPoint::Center, 0.0f, -230.0f});
    ecs.addComponent<engine::Tween>(title, pingPong(engine::TweenTarget::Scale, 1.0f, 1.05f, 1.6f));
    styledText(interface, ecs, "A BRICK BREAKER MADE WITH THE R-TYPE ENGINE",
               {AnchorPoint::Center, 0.0f, -130.0f}, 26.0f, subtitleColor);
    styledText(interface, ecs, "BEST SCORE  " + std::to_string(match.best),
               {AnchorPoint::Center, 0.0f, -60.0f}, 34.0f, Color{255, 220, 140, 255});
    interface.button("Play", [&target] { target.switchScene(GameScene); },
                     {AnchorPoint::Center, 0.0f, 50.0f});
    interface.button("Quit", quit, {AnchorPoint::Center, 0.0f, 50.0f + buttonSpacing});
    styledText(interface, ecs, "ARROWS  MOVE      SPACE  LAUNCH      ESCAPE  PAUSE",
               {AnchorPoint::Bottom, 0.0f, -200.0f}, 22.0f, subtitleColor);
}

void buildGame(engine::Engine &target, engine::SceneId scene, const engine::InterfaceStyle &style,
               const Looks &looks, Match &match)
{
    match = Match{.best = match.best};
    ecs::Ecs &ecs = target.getEcs();
    spawnSky(ecs, scene, looks);
    spawnWalls(ecs, scene);
    spawnPaddle(ecs, scene, looks);
    spawnBall(ecs, scene, looks);
    spawnBricks(ecs, scene, match.level);
    engine::InterfaceBuilder interface(ecs, scene, style);
    buildHud(interface, ecs, scene, match);
    spawnBanner(ecs, scene, looks, "LEVEL 1");
}

/// A dark veil over the arena, under the texts of an overlay. Wider than the arena, for wide screens.
void dimBelow(ecs::Ecs &ecs, engine::SceneId scene)
{
    const ecs::entity_t veil =
        spawnDrawn(ecs, scene, ArenaWidth / 2.0f, ArenaHeight / 2.0f, engine::Layer::Interface, dimOrder);
    ecs.addComponent<engine::Box>(veil, 3.0f * ArenaWidth, 3.0f * ArenaHeight, dimColor);
}

void buildPause(engine::Engine &target, engine::SceneId scene, const engine::InterfaceStyle &style)
{
    dimBelow(target.getEcs(), scene);
    engine::InterfaceBuilder interface(target.getEcs(), scene, style);
    interface.title("PAUSE", {AnchorPoint::Center, 0.0f, -buttonSpacing});
    interface.button("Resume", [&target] { target.popScene(); }, {AnchorPoint::Center, 0.0f, 20.0f});
    interface.button("Menu", [&target] { target.switchScene(MenuScene); },
                     {AnchorPoint::Center, 0.0f, 20.0f + buttonSpacing});
}

/// Keeps the score of the game if it is the best one, and writes it.
void keepBestScore(engine::Settings &settings, Match &match)
{
    if (match.score <= match.best) {
        return;
    }
    match.best = match.score;
    settings.setInt(BestScoreKey, match.best);
    try {
        settings.writeFile();
    } catch (const std::exception &error) {
        rtype::log::Logger("orbit").error("Cannot write the best score: {}", error.what());
    }
}

void buildGameOver(engine::Engine &target, engine::SceneId scene, const engine::InterfaceStyle &style,
                   Match &match)
{
    const bool record = match.score > match.best;
    keepBestScore(target.getSettings(), match);
    ecs::Ecs &ecs = target.getEcs();
    dimBelow(ecs, scene);
    engine::InterfaceBuilder interface(ecs, scene, style);
    popIn(ecs, interface.title(record ? "NEW BEST SCORE" : "GAME OVER", {AnchorPoint::Center, 0.0f, -250.0f}),
          0.6f, 0.5f);
    styledText(interface, ecs, std::to_string(match.score), {AnchorPoint::Center, 0.0f, -130.0f}, 90.0f,
               Color{255, 220, 140, 255});
    styledText(interface, ecs,
               "LEVEL " + std::to_string(match.level) + "      BEST " + std::to_string(match.best),
               {AnchorPoint::Center, 0.0f, -50.0f}, 30.0f, subtitleColor);
    interface.button("Play again", [&target] { target.switchScene(GameScene); },
                     {AnchorPoint::Center, 0.0f, 60.0f});
    interface.button("Menu", [&target] { target.switchScene(MenuScene); },
                     {AnchorPoint::Center, 0.0f, 60.0f + buttonSpacing});
}

} // namespace

void createScenes(engine::Engine &engine, const engine::InterfaceStyle &style, const Looks &looks,
                  Match &match, std::function<void()> quit)
{
    engine.createScene(
        MenuScene, {.onBack = [] {}},
        [&style, &looks, &match, quit = std::move(quit)](engine::Engine &target, engine::SceneId scene) {
            buildMenu(target, scene, style, looks, match, quit);
        });
    engine.createScene(GameScene, {.onBack = [&engine] { engine.pushScene(PauseScene); }},
                       [&style, &looks, &match](engine::Engine &target, engine::SceneId scene) {
                           buildGame(target, scene, style, looks, match);
                       });
    engine.createScene(
        PauseScene, {.overlay = true, .hidesInterfaceBelow = true},
        [&style](engine::Engine &target, engine::SceneId scene) { buildPause(target, scene, style); });
    engine.createScene(GameOverScene,
                       {.overlay = true,
                        .hidesInterfaceBelow = true,
                        .onBack = [&engine] { engine.switchScene(MenuScene); }},
                       [&style, &match](engine::Engine &target, engine::SceneId scene) {
                           buildGameOver(target, scene, style, match);
                       });
}

} // namespace orbit
