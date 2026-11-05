#pragma once

#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/localization/Localization.hpp"

namespace rtype::client
{

/// Registers the quests of the player, each waiting for its events (engine::Engine::addQuest()). A quest
/// completed is written to the settings file at once, and told by a toast with its name (`quest.<id>`).
/// @param translations It must outlive the engine.
void registerQuests(engine::Engine &engine, const localization::Localization &translations);

} // namespace rtype::client
