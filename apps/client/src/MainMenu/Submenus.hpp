#pragma once

#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/localization/Localization.hpp"

#include <functional>

namespace rtype::client
{

/// Names of the submenus of the main menu, opened by its buttons.
inline constexpr const char *PlayMenuScene = "play_menu";
inline constexpr const char *OnlineMenuScene = "online_menu";

/// Declares the submenus of the main menu. Each one opens over the main menu, whose world stays visible but
/// not its buttons, and shows its title after a back button that closes it, like Escape. Play offers the
/// campaign (CampaignScene), a game offline, the hangar, the story of the game (LoreScene) and the quests
/// (QuestsMenuScene). The Settings page is declared by createSettingsScene(), the Online menu by OnlineScenes.
/// @param style Look of the interface. It must outlive the engine.
/// @param playSolo Starts a game offline.
/// @param openHangar Opens the hangar, to look at the ships and equip one.
void createSubmenuScenes(engine::Engine &engine, const engine::InterfaceStyle &style,
                         localization::Localization &translations, std::function<void()> playSolo,
                         std::function<void()> openHangar);

} // namespace rtype::client
