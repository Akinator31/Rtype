#pragma once

#include "Game/GameScenes.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Settings/Settings.hpp"
#include "rtype/game/Config/GameConfig.hpp"
#include "rtype/game/Rules.hpp"
#include "rtype/localization/Localization.hpp"

#include <cstdint>
#include <string_view>

namespace rtype::client
{

/// Where the first mission of the campaign is kept as completed in the settings of the client.
inline constexpr std::string_view FirstMissionSettingsKey = "campaign.shogun.completed";

/// The name of the only wave of the first mission.
inline constexpr std::string_view FirstMissionWave = "campaign_first_contact";

/// Ticks of the voice played as the first mission starts (RTYPE_SOUND_CAMPAIGN_START): its first wave comes
/// as it ends. Keep it the length of the file.
inline constexpr std::uint32_t FirstMissionVoiceTicks = 810;

/// The definitions of the first mission: those of the game, its waves replaced by a single easy one, a few
/// Bydos that come from the right. Once they are all destroyed or gone, the mission is won.
[[nodiscard]] game::GameConfig firstMissionConfig(game::GameConfig config);

/// The rules of the first mission: its wave waits for the end of the voice.
[[nodiscard]] game::Rules firstMissionRules();

/// Whether the first mission was completed, as the settings remember it.
[[nodiscard]] bool isFirstMissionCompleted(const engine::Settings &settings);

/// Keeps the first mission as completed, and writes the settings.
/// @return False if the settings could not be written: it is then only kept until the client closes.
bool completeFirstMission(engine::Settings &settings);

/// Starts the first mission, the ship equipped straight in space, over the voice of its start. When the
/// game ends, a victory keeps the mission as completed (completeFirstMission()) and says so; then back to the
/// main menu.
/// @param games, config, translations They must outlive the game.
void playFirstMission(engine::Engine &engine, GameScenes &games, const game::GameConfig &config,
                      const localization::Localization &translations);

} // namespace rtype::client
