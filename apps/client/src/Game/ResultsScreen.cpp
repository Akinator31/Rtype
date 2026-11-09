#include "Game/ResultsScreen.hpp"

#include "Game/HudSystem.hpp"
#include "MainMenu/Capitals.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Layout.hpp"
#include "rtype/game/Progression.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <utility>

namespace rtype::client
{

namespace
{

/// Over the sky, under the buttons and the toasts.
constexpr int VeilOrder = 10;
constexpr int ContentOrder = 11;

constexpr render::Color VeilColor{0, 0, 0, 150};
constexpr render::Color CaptionColor{201, 205, 210, 255};
constexpr render::Color HintColor{107, 113, 119, 255};
/// The colour of the experience, as in the HUD.
constexpr render::Color XpColor{255, 214, 102, 255};
constexpr render::Color BarBackColor{255, 255, 255, 36};

/// Heights from the centre of the screen, and sizes.
constexpr float TitleY = -330.0f;
constexpr float TitleSize = 64.0f;
constexpr float TitleSpacing = 8.0f;
constexpr float FiguresY = -150.0f;
constexpr float FigureSize = 56.0f;
constexpr float FiguresGap = 360.0f;
constexpr float CaptionBelowFigure = 56.0f;
constexpr float CaptionSize = 18.0f;
constexpr float CaptionSpacing = 4.3f;
constexpr float XpY = 30.0f;
constexpr float XpSize = 80.0f;
constexpr float XpCaptionY = 92.0f;
constexpr float LevelY = 170.0f;
constexpr float LevelSize = 26.0f;
constexpr float LevelSpacing = 4.0f;
constexpr float BarY = 212.0f;
constexpr float BarHeight = 12.0f;
constexpr float LevelXpY = 248.0f;
constexpr float LevelUpY = 300.0f;
constexpr float LevelUpSize = 34.0f;

/// An entity of the results screen, centred at an offset from the centre of the screen, invisible until the
/// ResultsSystem fades it in.
ecs::entity_t createPart(ecs::Ecs &ecs, engine::SceneId scene, int order, render::Vector2 offset)
{
    const ecs::entity_t part = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(part, scene);
    ecs.addComponent<engine::Anchor>(part, engine::AnchorPoint::Center, offset.x, offset.y);
    ecs.addComponent<ecs::Position>(part);
    ecs.addComponent<engine::Drawable>(part, engine::Layer::Interface, order);
    ecs.addComponent<engine::Opacity>(part, 0.0f);
    return part;
}

ecs::entity_t createText(ecs::Ecs &ecs, engine::SceneId scene, render::Vector2 offset, std::string text,
                         render::FontId font, float size, render::Color color, float spacing = 0.0f)
{
    const ecs::entity_t part = createPart(ecs, scene, ContentOrder, offset);
    ecs.addComponent<engine::Text>(part, std::move(text), font, size, color, spacing);
    return part;
}

/// The figures of the game, side by side, each with its caption: they land one after the other.
void createFigures(ecs::Ecs &ecs, engine::SceneId scene, const ResultsLook &look, const GameResults &game,
                   ResultsScreen &screen)
{
    const std::array figures{
        std::pair{std::to_string(game.wavesCleared), look.waves},
        std::pair{std::to_string(game.kills), look.kills},
        std::pair{std::format("x{}", game.bestStreak), look.streak},
        std::pair{formatScore(game.score), look.score},
    };
    const float left = -FiguresGap * static_cast<float>(figures.size() - 1) / 2.0f;
    for (std::size_t index = 0; index < figures.size(); ++index) {
        const float x = left + (FiguresGap * static_cast<float>(index));
        const ecs::entity_t figure = createText(ecs, scene, {x, FiguresY}, figures[index].first,
                                                look.valueFont, FigureSize, render::colors::White);
        ecs.addComponent<engine::Scale>(figure, 1.0f);
        screen.figures.push_back(figure);
        screen.captions.push_back(createText(ecs, scene, {x, FiguresY + CaptionBelowFigure},
                                             toCapitals(figures[index].second), look.captionFont, CaptionSize,
                                             CaptionColor, CaptionSpacing));
    }
}

/// The level of the account, the bar of the level and the experience into it, written by the ResultsSystem.
void createLevel(ecs::Ecs &ecs, engine::SceneId scene, const ResultsLook &look, ResultsScreen &screen)
{
    screen.level = createText(ecs, scene, {0.0f, LevelY}, std::string(), look.valueFont, LevelSize,
                              render::colors::White, LevelSpacing);
    screen.barBack = createPart(ecs, scene, ContentOrder, {0.0f, BarY});
    ecs.addComponent<engine::Box>(screen.barBack, ResultsBarWidth, BarHeight, BarBackColor);
    screen.barFill = createPart(ecs, scene, ContentOrder + 1, {-ResultsBarWidth / 2.0f, BarY});
    ecs.addComponent<engine::Box>(screen.barFill, 0.0f, BarHeight, XpColor);
    screen.levelXp = createText(ecs, scene, {0.0f, LevelXpY}, std::string(), look.captionFont, CaptionSize,
                                CaptionColor, CaptionSpacing);
    screen.levelUp = createText(ecs, scene, {0.0f, LevelUpY}, toCapitals(look.levelUp), look.valueFont,
                                LevelUpSize, XpColor, LevelSpacing);
    ecs.addComponent<engine::Scale>(screen.levelUp, 1.0f);
    screen.experience.insert(screen.experience.end(),
                             {screen.level, screen.barBack, screen.barFill, screen.levelXp});
}

} // namespace

ecs::entity_t openResultsScreen(ecs::Ecs &ecs, engine::SceneId scene, const engine::Metrics &metrics,
                                const ResultsLook &look, const GameResults &game,
                                std::optional<std::uint64_t> accountXp)
{
    ResultsScreen screen;
    screen.game = game;
    screen.before = accountXp;
    screen.after = accountXp.value_or(0) + game.xp;
    screen.sounds = look.sounds;
    screen.levelCaption = look.level;
    screen.levelShown = game::levelOf(accountXp.value_or(0)).level;
    const render::Rectangle area = metrics.visibleArea();

    screen.veil = createPart(ecs, scene, VeilOrder, {0.0f, 0.0f});
    ecs.addComponent<engine::Box>(screen.veil, area.width, area.height, VeilColor);

    screen.title = createText(ecs, scene, {0.0f, TitleY}, look.title, look.valueFont, TitleSize,
                              render::colors::White, TitleSpacing);
    ecs.addComponent<engine::Title>(screen.title);
    createFigures(ecs, scene, look, game, screen);

    // Written by the ResultsSystem as it counts
    screen.xp = createText(ecs, scene, {0.0f, XpY}, "+0 XP", look.valueFont, XpSize, XpColor);
    ecs.addComponent<engine::Scale>(screen.xp, 1.0f);
    screen.xpCaption = createText(ecs, scene, {0.0f, XpCaptionY}, toCapitals(look.xp), look.captionFont,
                                  CaptionSize, CaptionColor, CaptionSpacing);
    screen.experience = {screen.xp, screen.xpCaption};
    if (accountXp) {
        createLevel(ecs, scene, look, screen);
    } else if (!look.guest.empty()) {
        screen.guest = createText(ecs, scene, {0.0f, LevelY}, look.guest, look.captionFont, CaptionSize,
                                  HintColor, CaptionSpacing);
    }

    const ecs::entity_t entity = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(entity, scene);
    ecs.addComponent<ResultsScreen>(entity, std::move(screen));
    return entity;
}

void setAccountXp(ResultsScreen &screen, std::uint64_t xp)
{
    if (screen.before) {
        screen.after = std::max(xp, *screen.before);
    }
}

} // namespace rtype::client
