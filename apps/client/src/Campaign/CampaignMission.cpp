#include "Campaign/CampaignMission.hpp"

#include "Assets.hpp"
#include "Campaign/CampaignScene.hpp"
#include "Game/LocalSession.hpp"
#include "MainMenu/MainMenu.hpp"
#include "rtype/game/Wave/WaveDefinition.hpp"
#include "rtype/log/Logger.hpp"

#include <exception>
#include <format>
#include <memory>
#include <utility>

namespace rtype::client
{

namespace
{

/// Ticks of the voice of the start of the first mission (RTYPE_SOUND_CAMPAIGN_START): keep it the length of
/// the file.
constexpr std::uint32_t StartVoiceTicks = 810;

/// Ticks before the wave of a mission without a voice.
constexpr std::uint32_t WaveDelay = game::Rules{}.firstWaveDelay;

/// The first mission, a few Bydos that scout the edge of the system: scouts in pairs, high and low, then the
/// wanderers in the middle. Then the road to Saturn, guarded by more of them: mosquitos in a swarm around
/// the scouts, and a gunner behind each group.
const std::vector<CampaignMission> Missions{
    {.id = "shogun",
     .briefingFrench = RTYPE_CAMPAIGN_BRIEFING_FRENCH,
     .briefingEnglish = RTYPE_CAMPAIGN_BRIEFING_ENGLISH,
     .voice = RTYPE_SOUND_CAMPAIGN_START,
     .firstWaveDelay = StartVoiceTicks,
     .wave = "campaign_first_contact",
     .label = "First contact",
     .spawns = {{"bydo_scout", 2000.0f, 300.0f},
                {"bydo_scout", 2000.0f, 780.0f},
                {"bydo_wanderer", 2000.0f, 540.0f},
                {"bydo_scout", 2000.0f, 200.0f},
                {"bydo_scout", 2000.0f, 880.0f},
                {"bydo_wanderer", 2000.0f, 540.0f}},
     .spawnInterval = 0.9f},
    {.id = "saturn",
     .briefingFrench = RTYPE_CAMPAIGN_SATURN_FRENCH,
     .briefingEnglish = RTYPE_CAMPAIGN_SATURN_ENGLISH,
     .voice = {},
     .firstWaveDelay = WaveDelay,
     .wave = "campaign_road_to_saturn",
     .label = "The road to Saturn",
     .spawns = {{"bydo_mosquito", 2000.0f, 250.0f},
                {"bydo_mosquito", 2000.0f, 350.0f},
                {"bydo_scout", 2000.0f, 300.0f},
                {"bydo_gunner", 2000.0f, 300.0f},
                {"bydo_mosquito", 2000.0f, 730.0f},
                {"bydo_mosquito", 2000.0f, 830.0f},
                {"bydo_scout", 2000.0f, 780.0f},
                {"bydo_gunner", 2000.0f, 780.0f},
                {"bydo_wanderer", 2000.0f, 540.0f},
                {"bydo_wanderer", 2000.0f, 540.0f}},
     .spawnInterval = 0.7f},
};

} // namespace

const std::vector<CampaignMission> &campaignMissions()
{
    return Missions;
}

std::string missionSettingsKey(const CampaignMission &mission)
{
    return std::format("campaign.{}.completed", mission.id);
}

bool isMissionCompleted(const engine::Settings &settings, const CampaignMission &mission)
{
    return settings.getBool(missionSettingsKey(mission)).value_or(false);
}

bool completeMission(engine::Settings &settings, const CampaignMission &mission)
{
    settings.setBool(missionSettingsKey(mission), true);
    try {
        settings.writeFile();
    } catch (const std::exception &error) {
        log::Logger("campaign")
            .error("Cannot write the settings {}: {}", settings.getPath().string(), error.what());
        return false;
    }
    return true;
}

std::size_t firstMissionToPlay(const engine::Settings &settings)
{
    for (std::size_t index = 0; index < Missions.size(); ++index) {
        if (!isMissionCompleted(settings, Missions[index])) {
            return index;
        }
    }
    return 0;
}

game::GameConfig missionConfig(const CampaignMission &mission, game::GameConfig config)
{
    game::WaveDefinition wave;
    wave.name = std::string(mission.wave);
    wave.label = std::string(mission.label);
    wave.difficulty = game::MinWaveDifficulty;
    wave.placement = game::WavePlacement::Manual;
    wave.spawnInterval = mission.spawnInterval;
    wave.spawns = mission.spawns;
    wave.quantity = static_cast<std::uint16_t>(wave.spawns.size());
    config.waves = {std::move(wave)};
    return config;
}

game::Rules missionRules(const CampaignMission &mission)
{
    game::Rules rules;
    rules.firstWaveDelay = mission.firstWaveDelay;
    return rules;
}

Campaign::Campaign(engine::Engine &engine, GameScenes &games, const game::GameConfig &config,
                   const localization::Localization &translations)
    : _engine(engine), _games(games), _config(config), _translations(translations)
{
}

const CampaignMission &Campaign::brief()
{
    const std::optional<std::size_t> next = std::exchange(_next, std::nullopt);
    _fromMenu = !next.has_value();
    _current = next.value_or(firstMissionToPlay(_engine.getSettings()));
    return Missions[_current];
}

const CampaignMission &Campaign::current() const
{
    return Missions[_current];
}

void Campaign::play()
{
    const CampaignMission &mission = current();
    _games.play(
        std::make_unique<LocalSession>(missionRules(mission), missionConfig(mission, _config)),
        [this] { end(); }, std::string(mission.voice), mission.firstWaveDelay);
}

void Campaign::leave()
{
    if (_fromMenu) {
        _engine.popScene();
    } else {
        _engine.switchScene(MainMenuScene);
    }
}

void Campaign::end()
{
    if (_games.outcome() != game::GameOutcome::Victory) {
        _engine.switchScene(MainMenuScene);
        return;
    }
    completeMission(_engine.getSettings(), current());
    if (_current + 1 < Missions.size()) {
        // The briefing of the next mission, which follows on its own
        _next = _current + 1;
        _engine.switchScene(CampaignScene);
        return;
    }
    _engine.showToast(_translations.text("campaign.completed"), engine::ToastType::Success);
    _engine.switchScene(MainMenuScene);
}

} // namespace rtype::client
