#pragma once

#include "Game/GameScenes.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Settings/Settings.hpp"
#include "rtype/game/Config/GameConfig.hpp"
#include "rtype/game/Rules.hpp"
#include "rtype/game/Wave/WaveDefinition.hpp"
#include "rtype/game/Wave/WaveSpawn.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rtype::client
{

/// A mission of the campaign: its briefing, received as a transmission (CampaignScene), then a single wave,
/// the ship equipped straight in space. Once its Bydos are all destroyed or gone, the mission is won. A
/// wave of the difficulty of the final boss (game::MaxWaveDifficulty) is the duel against the shogun: when
/// the last ship is lost against it, the allies come to the rescue (game::Rules::rescue), and its fall wins
/// it.
struct CampaignMission {
    /// Unique, it names the mission in the settings (missionSettingsKey()).
    std::string_view id;
    /// The briefing in each language, a path of the assets.
    const char *briefingFrench;
    const char *briefingEnglish;
    /// The voice played once in space, a path of the assets: none if empty.
    std::string_view voice;
    /// Ticks before the wave comes: the length of the voice, if any.
    std::uint32_t firstWaveDelay;
    /// The wave: its name and label, and its Bydos in the order they come, one every spawnInterval seconds.
    std::string_view wave;
    std::string_view label;
    std::vector<game::WaveSpawn> spawns;
    float spawnInterval = 1.0f;
    /// How hard the wave is: game::MaxWaveDifficulty for the duel against the shogun.
    std::uint8_t difficulty = game::MinWaveDifficulty;
    /// The music of the wave, a path of the assets: the music playing goes on if empty.
    std::string_view music{};
    /// The voices played one after the other when the last ship is lost against the shogun, until the allies
    /// come: their lengths are the wait of the rescue (game::Rules::rescueVoiceTicks). One without a path is
    /// a silence.
    std::vector<GameVoice> rescueVoices{};
    /// The ticks into the rescue voices at which the ship of the player comes back, as a cutscene until they
    /// end (GameVoices::rescueCutscene), and at which an escort flies in next to it: with the allies if 0
    /// (game::Rules::rescueShipTicks, rescueEscortTicks).
    std::uint32_t rescueShipTicks = 0;
    std::uint32_t rescueEscortTicks = 0;
    /// The voice played as the shogun falls: the victory waits for its end
    /// (game::Rules::rescueVictoryDelay).
    GameVoice victoryVoice{};
};

/// The missions of the campaign, in the order they are played.
[[nodiscard]] const std::vector<CampaignMission> &campaignMissions();

/// Where a mission is kept as completed in the settings of the client: "campaign.<id>.completed".
[[nodiscard]] std::string missionSettingsKey(const CampaignMission &mission);

/// Whether a mission was completed, as the settings remember it.
[[nodiscard]] bool isMissionCompleted(const engine::Settings &settings, const CampaignMission &mission);

/// Keeps a mission as completed, and writes the settings.
/// @return False if the settings could not be written: it is then only kept until the client closes.
bool completeMission(engine::Settings &settings, const CampaignMission &mission);

/// The index of the first mission not completed yet: the first one again once they all are.
[[nodiscard]] std::size_t firstMissionToPlay(const engine::Settings &settings);

/// The definitions of a mission: those of the game, its waves replaced by the single one of the mission.
[[nodiscard]] game::GameConfig missionConfig(const CampaignMission &mission, game::GameConfig config);

/// The rules of a mission: its wave waits for the end of its voice, the allies of the rescue for the end of
/// its rescue voices, and the victory for the end of its victory voice.
[[nodiscard]] game::Rules missionRules(const CampaignMission &mission);

/// The voices of the game of a mission (GameScenes::play()).
[[nodiscard]] GameVoices missionVoices(const CampaignMission &mission);

/// The campaign as it is played: the mission whose briefing opens, then its game. A mission won is kept as
/// completed (completeMission()), and the briefing of the next one follows; after the last one, its epilogue
/// (CampaignEndingScene), then the main menu. A mission lost or left goes back to the main menu.
///
/// From the Play menu, the briefing is that of the first mission not completed (firstMissionToPlay()).
class Campaign
{
    engine::Engine &_engine;
    GameScenes &_games;
    const game::GameConfig &_config;
    /// The mission whose briefing opens next, when a mission won leads to it.
    std::optional<std::size_t> _next;
    /// The mission briefed, then played.
    std::size_t _current = 0;
    /// The briefing opened over the menus, from the Play menu, rather than after a mission.
    bool _fromMenu = true;

    /// The game of the current mission ended.
    void end();

public:
    /// Everything given must outlive the campaign.
    Campaign(engine::Engine &engine, GameScenes &games, const game::GameConfig &config);

    /// The mission of the briefing that opens, which becomes the current one: called as the scene of the
    /// briefing is built.
    [[nodiscard]] const CampaignMission &brief();
    /// The current mission, briefed.
    [[nodiscard]] const CampaignMission &current() const;

    /// Plays the current mission, once its briefing is over.
    void play();
    /// Leaves the briefing: back to the menu it opened over, or to the main menu after a mission.
    void leave();
};

} // namespace rtype::client
