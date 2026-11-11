#pragma once

#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Settings/Settings.hpp"
#include "rtype/localization/Localization.hpp"

#include <string>
#include <vector>

namespace rtype::client
{

/// Registers the quests of the player, each waiting for its events (engine::Engine::addQuest()). A quest
/// completed is written to the settings file at once, and told by a toast with its name (`quest.<id>`).
/// @param translations It must outlive the engine.
void registerQuests(engine::Engine &engine, const localization::Localization &translations);

/// Where the player is in a quest: its stat against its target, or 0 / 1 for a quest completed by opening a
/// scene. Its name is translated by `quest.<id>`, its goal by `quest_goal.<id>`.
struct QuestProgress {
    std::string id;
    int current = 0;
    int target = 1;
    bool completed = false;
};

/// Every quest of the player, in the order they are registered, as the settings tell.
[[nodiscard]] std::vector<QuestProgress> getQuestProgress(const engine::Settings &settings);

} // namespace rtype::client
