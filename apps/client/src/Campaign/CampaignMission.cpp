#include "Campaign/CampaignMission.hpp"

#include "Assets.hpp"
#include "Game/LocalSession.hpp"
#include "MainMenu/MainMenu.hpp"
#include "rtype/game/Wave/WaveDefinition.hpp"
#include "rtype/log/Logger.hpp"

#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace rtype::client
{

namespace
{

/// The Bydos of the first wave, from the right of the game area, in the order they come: scouts in pairs,
/// high and low, then the wanderers in the middle.
const std::vector<game::WaveSpawn> FirstWaveSpawns{
    {"bydo_scout", 2000.0f, 300.0f}, {"bydo_scout", 2000.0f, 780.0f}, {"bydo_wanderer", 2000.0f, 540.0f},
    {"bydo_scout", 2000.0f, 200.0f}, {"bydo_scout", 2000.0f, 880.0f}, {"bydo_wanderer", 2000.0f, 540.0f},
};

/// Seconds between two Bydos of the first wave.
constexpr float FirstWaveInterval = 0.9f;

} // namespace

game::GameConfig firstMissionConfig(game::GameConfig config)
{
    game::WaveDefinition wave;
    wave.name = std::string(FirstMissionWave);
    wave.label = "First contact";
    wave.description = "A few Bydos scout the edge of the system";
    wave.difficulty = game::MinWaveDifficulty;
    wave.placement = game::WavePlacement::Manual;
    wave.spawnInterval = FirstWaveInterval;
    wave.spawns = FirstWaveSpawns;
    wave.quantity = static_cast<std::uint16_t>(wave.spawns.size());
    config.waves = {std::move(wave)};
    return config;
}

game::Rules firstMissionRules()
{
    game::Rules rules;
    rules.firstWaveDelay = FirstMissionVoiceTicks;
    return rules;
}

bool isFirstMissionCompleted(const engine::Settings &settings)
{
    return settings.getBool(FirstMissionSettingsKey).value_or(false);
}

bool completeFirstMission(engine::Settings &settings)
{
    settings.setBool(FirstMissionSettingsKey, true);
    try {
        settings.writeFile();
    } catch (const std::exception &error) {
        log::Logger("campaign")
            .error("Cannot write the settings {}: {}", settings.getPath().string(), error.what());
        return false;
    }
    return true;
}

void playFirstMission(engine::Engine &engine, GameScenes &games, const game::GameConfig &config,
                      const localization::Localization &translations)
{
    games.play(
        std::make_unique<LocalSession>(firstMissionRules(), firstMissionConfig(config)),
        [&engine, &games, &translations] {
            if (games.outcome() == game::GameOutcome::Victory) {
                completeFirstMission(engine.getSettings());
                engine.showToast(translations.text("campaign.completed"), engine::ToastType::Success);
            }
            engine.switchScene(MainMenuScene);
        },
        RTYPE_SOUND_CAMPAIGN_START);
}

} // namespace rtype::client
