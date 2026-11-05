#include "Quests/Quests.hpp"

#include "Game/GameEvents.hpp"
#include "Quests/EventQuest.hpp"
#include "rtype/log/Logger.hpp"

#include <exception>

namespace rtype::client
{

namespace
{

void writeSettings(const engine::Settings &settings)
{
    try {
        settings.writeFile();
    } catch (const std::exception &error) {
        const log::Logger logger("client");
        logger.warning("Cannot write the settings {}: {}", settings.getPath().string(), error.what());
    }
}

void addEventQuest(engine::Engine &engine, const localization::Localization &translations,
                   const std::string &id, const std::string &event)
{
    EventQuest &quest = engine.getEcs().registerSystem<EventQuest>(id, event, engine.getSettings());
    quest.setCompletionHandler([&engine, &translations](const engine::QuestSystem &completed) {
        writeSettings(engine.getSettings());
        engine.showBanner(translations.text("quest.completed"), translations.text(completed.getSettingsKey()),
                          engine::ToastType::Quest);
    });
    engine.addQuest(quest);
}

} // namespace

void registerQuests(engine::Engine &engine, const localization::Localization &translations)
{
    addEventQuest(engine, translations, "first_blood", BydoKilledEvent);
    addEventQuest(engine, translations, "boss_slayer", BossKilledEvent);
}

} // namespace rtype::client
