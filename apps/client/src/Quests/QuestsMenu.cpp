#include "Quests/QuestsMenu.hpp"

#include "Game/GameRecorder.hpp"
#include "MainMenu/Layout.hpp"
#include "MainMenu/SettingsPage.hpp"
#include "Quests/Quests.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace rtype::client
{

namespace
{

/// The progress of the game, under the header: the gauge of the quests completed, then the stats on its
/// right.
constexpr float progressTop = 176.0f;
constexpr float statsGap = 80.0f;
constexpr float statsTopOffset = 10.0f;

/// The cards of the quests, in a grid of `gridColumns` columns that fills the width of the game between the
/// margins of the screen.
constexpr float gameWidth = 1920.0f;
constexpr float gridTop = 290.0f;
constexpr int gridColumns = 4;
constexpr float cardGap = 24.0f;
constexpr float rowGap = 16.0f;
constexpr float cardWidth =
    (gameWidth - (2.0f * screenMargin) - (static_cast<float>(gridColumns - 1) * cardGap)) / gridColumns;
constexpr float cardHeight = 100.0f;

/// Inside a card: its name, its goal under it, then its progress bar and its counter at the bottom.
constexpr float cardInset = 22.0f;
constexpr float nameTop = 14.0f;
constexpr float nameSize = 22.0f;
constexpr float nameSpacing = 2.5f;
constexpr float goalTop = 44.0f;
constexpr float goalSize = 19.0f;
constexpr float barTop = 80.0f;
constexpr float barWidth = 250.0f;
constexpr float barHeight = 3.0f;
constexpr float counterGap = 20.0f;
constexpr float counterTop = 71.0f;
constexpr float counterSize = 18.0f;
constexpr float counterSpacing = 2.0f;
/// The mark on the left edge of a completed card.
constexpr float markWidth = 4.0f;

/// The drawing orders in the interface layer: the veil (settings_page::veilOrder), the cards, their content,
/// then the light of their bars over their rails.
constexpr int contentOrder = 2;
constexpr int lightOrder = 3;

constexpr render::Color lockedCard{255, 255, 255, 8};
constexpr render::Color completedCard{96, 170, 255, 22};
constexpr render::Color lockedName{174, 179, 184, 255};
constexpr render::Color rail{38, 41, 45, 255};
constexpr render::Color lockedLight{255, 255, 255, 200};

/// A counter of a stat, short enough for a card: 37K for 37 210.
std::string formatCount(int count)
{
    constexpr int shortenFrom = 10000;
    if (count < shortenFrom) {
        return std::to_string(count);
    }
    return std::format("{}K", count / 1000);
}

/// The name of a quest without its target (`BYDO HUNTER · 50 BYDOS`): the counter of the card tells it.
std::string getShortName(const std::string &name)
{
    return name.substr(0, name.find(" · "));
}

[[nodiscard]] float getRatio(const QuestProgress &quest)
{
    if (quest.completed) {
        return 1.0f;
    }
    return std::clamp(static_cast<float>(quest.current) / static_cast<float>(quest.target), 0.0f, 1.0f);
}

/// A text of a card, drawn over it.
ecs::entity_t addCardText(engine::InterfaceBuilder &interface, ecs::Ecs &ecs, const std::string &text,
                          render::Vector2 place, float size, render::Color color, float spacing)
{
    const ecs::entity_t entity = interface.text(text, {engine::AnchorPoint::TopLeft, place.x, place.y});
    engine::Text &content = ecs.getComponent<engine::Text>(entity);
    content.size = size;
    content.color = color;
    content.spacing = spacing;
    ecs.getComponent<engine::Drawable>(entity).order = contentOrder;
    return entity;
}

/// A box of a card, drawn over it at `order`.
void addCardBox(engine::InterfaceBuilder &interface, ecs::Ecs &ecs, render::Rectangle area,
                render::Color color, int order)
{
    const ecs::entity_t entity =
        interface.box(area.width, area.height, color, {engine::AnchorPoint::TopLeft, area.x, area.y});
    ecs.getComponent<engine::Drawable>(entity).order = order;
}

/// The progress bar of a card and its counter: `12 / 50`, or COMPLETED.
void addCardProgress(engine::InterfaceBuilder &interface, ecs::Ecs &ecs, const engine::InterfaceStyle &style,
                     const localization::Localization &translations, const QuestProgress &quest,
                     render::Vector2 corner)
{
    const float left = corner.x + cardInset;
    const float top = corner.y + barTop;
    const render::Color light = quest.completed ? style.toast.quest : lockedLight;
    addCardBox(interface, ecs, {left, top, barWidth, barHeight}, rail, contentOrder);
    const float lit = barWidth * getRatio(quest);
    if (lit > 0.0f) {
        addCardBox(interface, ecs, {left, top, lit, barHeight}, light, lightOrder);
    }
    const std::string counter =
        quest.completed ? translations.text("quest.done")
                        : std::format("{} / {}", formatCount(quest.current), formatCount(quest.target));
    addCardText(interface, ecs, counter, {left + barWidth + counterGap, corner.y + counterTop}, counterSize,
                quest.completed ? style.toast.quest : lockedName, counterSpacing);
}

/// The card of a quest whose top left corner is `corner`.
void addQuestCard(engine::InterfaceBuilder &interface, ecs::Ecs &ecs, const engine::InterfaceStyle &style,
                  const localization::Localization &translations, const QuestProgress &quest,
                  render::Vector2 corner)
{
    addCardBox(interface, ecs, {corner.x, corner.y, cardWidth, cardHeight},
               quest.completed ? completedCard : lockedCard, 1);
    if (quest.completed) {
        addCardBox(interface, ecs, {corner.x, corner.y, markWidth, cardHeight}, style.toast.quest,
                   contentOrder);
    }
    const float left = corner.x + cardInset;
    addCardText(interface, ecs, getShortName(translations.text("quest." + quest.id)),
                {left, corner.y + nameTop}, nameSize, quest.completed ? render::colors::White : lockedName,
                nameSpacing);
    const ecs::entity_t goal =
        interface.note(translations.text("quest_goal." + quest.id),
                       {engine::AnchorPoint::TopLeft, left, corner.y + goalTop});
    ecs.getComponent<engine::Text>(goal).size = goalSize;
    ecs.getComponent<engine::Drawable>(goal).order = contentOrder;
    addCardProgress(interface, ecs, style, translations, quest, corner);
}

void addQuestCards(engine::InterfaceBuilder &interface, ecs::Ecs &ecs, const engine::InterfaceStyle &style,
                   const localization::Localization &translations, const std::vector<QuestProgress> &quests)
{
    for (std::size_t index = 0; index < quests.size(); ++index) {
        const auto column = static_cast<float>(index % gridColumns);
        const auto row = static_cast<float>(index / gridColumns);
        const render::Vector2 corner{screenMargin + (column * (cardWidth + cardGap)),
                                     gridTop + (row * (cardHeight + rowGap))};
        addQuestCard(interface, ecs, style, translations, quests[index], corner);
    }
}

/// The main stats of the player, in one line: `GAMES 12 · BYDOS 340 · ...`.
std::string formatStats(const engine::Settings &settings, const localization::Localization &translations)
{
    const std::array<std::pair<const char *, const char *>, 4> stats{{
        {"quests.stats_games", GamesPlayedStat},
        {"quests.stats_bydos", BydosKilledStat},
        {"quests.stats_wave", BestWaveStat},
        {"quests.stats_score", BestScoreStat},
    }};
    std::string line;
    for (const auto &[key, stat] : stats) {
        if (!line.empty()) {
            line += "   ·   ";
        }
        line += std::format("{} {}", translations.text(key), getStat(settings, stat));
    }
    return line;
}

/// How far the player is in the game: a gauge of the quests completed, and the stats on its right.
void addProgress(engine::InterfaceBuilder &interface, ecs::Ecs &ecs, const engine::InterfaceStyle &style,
                 const engine::Settings &settings, const localization::Localization &translations,
                 const std::vector<QuestProgress> &quests)
{
    const auto completed =
        std::ranges::count_if(quests, [](const QuestProgress &quest) { return quest.completed; });
    const float ratio =
        quests.empty() ? 0.0f : static_cast<float>(completed) / static_cast<float>(quests.size());
    const std::string caption = std::format("{} · {} / {} {}", translations.text("quests.progress"), completed,
                                            quests.size(), translations.text("quests.completed_suffix"));
    const ecs::entity_t gauge = interface.gauge(caption, [ratio] { return ratio; },
                                                {engine::AnchorPoint::TopLeft, screenMargin, progressTop});
    ecs.getComponent<engine::Gauge>(gauge).showsPercentage = false;
    interface.note(formatStats(settings, translations),
                   {engine::AnchorPoint::TopLeft, screenMargin + style.gauge.width + statsGap,
                    progressTop + statsTopOffset});
}

void buildQuestsMenu(engine::Engine &engine, engine::SceneId scene, const engine::InterfaceStyle &style,
                     const localization::Localization &translations)
{
    ecs::Ecs &ecs = engine.getEcs();
    const std::vector<QuestProgress> quests = getQuestProgress(engine.getSettings());
    engine::InterfaceBuilder interface(ecs, scene, style);
    settings_page::addVeil(interface, ecs);
    interface.boundHeader([&translations] { return translations.text("menu.quests"); },
                          [&engine] { engine.popScene(); },
                          {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});
    addProgress(interface, ecs, style, engine.getSettings(), translations, quests);
    addQuestCards(interface, ecs, style, translations, quests);
}

} // namespace

void createQuestsMenuScene(engine::Engine &engine, const engine::InterfaceStyle &style,
                           const localization::Localization &translations)
{
    engine.createScene(QuestsMenuScene, {.overlay = true, .hidesInterfaceBelow = true},
                       [&style, &translations](engine::Engine &target, engine::SceneId scene) {
                           buildQuestsMenu(target, scene, style, translations);
                       });
}

} // namespace rtype::client
