#pragma once

#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/localization/Localization.hpp"

namespace rtype::client
{

/// Name of the quests page, opened from the Play submenu.
inline constexpr const char *QuestsMenuScene = "quests_menu";

/// Declares the quests page: under its header, how far the player is in the game (the quests completed out
/// of all of them, and their main stats), then every quest as a card, with its goal and its progress. The
/// completed ones are lit in the colour of the quests, the others dim. It reads the settings when it opens.
/// @param style, translations They must outlive the engine.
void createQuestsMenuScene(engine::Engine &engine, const engine::InterfaceStyle &style,
                           const localization::Localization &translations);

} // namespace rtype::client
