#include "Quests/Quests.hpp"

#include "Game/GameRecorder.hpp"
#include "Lore/LoreScene.hpp"
#include "MainMenu/SettingsMenu.hpp"
#include "Online/OnlineScenes.hpp"
#include "Quests/EventQuest.hpp"
#include "Quests/StatQuest.hpp"
#include "Settings/PlayerSettings.hpp"
#include "ShipSelector/ShipSelector.hpp"
#include "rtype/game/Wave/WaveDefinition.hpp"

#include <array>
#include <string>
#include <vector>

namespace rtype::client
{

namespace
{

/// A quest completed once a stat reaches a target, checked each time its event is triggered.
struct StatQuestDefinition {
    const char *id;
    const char *event;
    const char *stat;
    int target;
};

/// A quest completed the first time a scene opens.
struct SceneQuestDefinition {
    const char *id;
    const char *scene;
};

constexpr std::array<StatQuestDefinition, 20> StatQuests{{
    {"first_flight", GameStartedEvent, GamesPlayedStat, 1},
    {"veteran_pilot", GameStartedEvent, GamesPlayedStat, 25},
    {"first_blood", BydoKilledEvent, BydosKilledStat, 1},
    {"bydo_hunter", BydoKilledEvent, BydosKilledStat, 50},
    {"bydo_exterminator", BydoKilledEvent, BydosKilledStat, 250},
    {"bydo_nemesis", BydoKilledEvent, BydosKilledStat, 1000},
    {"flyer_swatter", BydoKilledEvent, FlyersKilledStat, 25},
    {"shooter_breaker", BydoKilledEvent, ShootersKilledStat, 25},
    {"tank_buster", BydoKilledEvent, TanksKilledStat, 10},
    {"boss_slayer", BossKilledEvent, BossesDefeatedStat, 1},
    {"boss_veteran", BossKilledEvent, BossesDefeatedStat, 5},
    {"baptism_of_fire", PlayerKilledEvent, DeathsStat, 1},
    {"third_wave", WaveStartedEvent, BestWaveStat, 3},
    {"boss_wave", WaveStartedEvent, BestWaveStat, game::MaxWaveDifficulty},
    {"first_victory", GameWonEvent, VictoriesStat, 1},
    {"ace", GameWonEvent, VictoriesStat, 10},
    {"never_give_up", GameLostEvent, DefeatsStat, 10},
    {"high_score", GameEndedEvent, BestScoreStat, 10000},
    {"legendary_score", GameEndedEvent, BestScoreStat, 50000},
    {"career", GameEndedEvent, TotalScoreStat, 100000},
}};

constexpr std::array<SceneQuestDefinition, 4> SceneQuests{{
    {"storyteller", LoreScene},
    {"hangar_visit", ShipSelectorScene},
    {"online_pilot", LobbyScene},
    {"tinkerer", SettingsMenuScene},
}};

/// Makes the quest wait for its events; once it is completed, writes the settings and shows its banner.
void addQuest(engine::Engine &engine, const localization::Localization &translations,
              engine::QuestSystem &quest)
{
    quest.setCompletionHandler([&engine, &translations](const engine::QuestSystem &completed) {
        writeSettings(engine.getSettings());
        engine.showBanner(translations.text("quest.completed"), translations.text(completed.getSettingsKey()),
                          engine::ToastType::Quest);
    });
    engine.addQuest(quest);
}

} // namespace

std::vector<QuestProgress> getQuestProgress(const engine::Settings &settings)
{
    std::vector<QuestProgress> quests;
    for (const StatQuestDefinition &quest : StatQuests) {
        quests.push_back({.id = quest.id,
                          .current = getStat(settings, quest.stat),
                          .target = quest.target,
                          .completed = settings.getBool(std::string("quest.") + quest.id).value_or(false)});
    }
    for (const SceneQuestDefinition &quest : SceneQuests) {
        const bool completed = settings.getBool(std::string("quest.") + quest.id).value_or(false);
        quests.push_back({.id = quest.id, .current = completed ? 1 : 0, .target = 1, .completed = completed});
    }
    return quests;
}

void registerQuests(engine::Engine &engine, const localization::Localization &translations)
{
    ecs::Ecs &ecs = engine.getEcs();
    for (const StatQuestDefinition &quest : StatQuests) {
        addQuest(engine, translations,
                 ecs.registerSystem<StatQuest>(quest.id, quest.event, quest.stat, quest.target,
                                               engine.getSettings()));
    }
    for (const SceneQuestDefinition &quest : SceneQuests) {
        addQuest(engine, translations,
                 ecs.registerSystem<EventQuest>(quest.id, std::string(quest.scene) + ":open",
                                                engine.getSettings()));
    }
}

} // namespace rtype::client
