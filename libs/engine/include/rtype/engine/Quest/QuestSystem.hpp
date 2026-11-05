#pragma once

#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Settings/Settings.hpp"

#include <functional>
#include <string>
#include <vector>

namespace rtype::engine
{

/// A quest of the player: a system that sleeps until one of its events is triggered. Engine::addQuest()
/// disables it and subscribes it to its events with Reaction::Enable and Duration::Once: each update after a
/// trigger, it runs once and calls progress(), which tells whether the quest is now completed.
///
/// A completed quest is written in the settings, as `quest.<id>=true`, and stays completed at the next runs
/// once the settings are written to their file (the completion handler does it). It never progresses again.
///
/// An event triggered several times before an update wakes the quest once: a quest that counts something
/// reads a counter rather than count its wakes.
/// @code
/// class FirstKillQuest : public QuestSystem
/// {
///     bool progress() override { return true; }
///
/// public:
///     FirstKillQuest(ecs::ComponentCaches &, Settings &settings)
///         : QuestSystem("first_kill", {"game:bydo_killed"}, settings) {}
/// };
/// engine.addQuest(engine.getEcs().registerSystem<FirstKillQuest>(engine.getSettings()));
/// @endcode
class QuestSystem : public rtype::ecs::System
{
    std::string _id;
    std::vector<std::string> _events;
    Settings &_settings;
    std::function<void(const QuestSystem &)> _onCompleted;

    void complete();

protected:
    /// Called each time one of the events of the quest wakes it, until it is completed.
    /// @return Whether the quest is completed.
    virtual bool progress() = 0;

public:
    /// @param id Names the quest in the settings: unique among the quests.
    /// @param events The events that wake the quest.
    /// @param settings Where the quests completed are written. It must outlive the quest.
    QuestSystem(std::string id, std::vector<std::string> events, Settings &settings);

    [[nodiscard]] const std::string &getId() const;
    [[nodiscard]] const std::vector<std::string> &getEvents() const;
    /// The key of the quest in the settings: `quest.<id>`.
    [[nodiscard]] std::string getSettingsKey() const;
    /// Completed now, or at a previous run as the settings tell.
    [[nodiscard]] bool isCompleted() const;

    /// Calls this once, when the quest is completed: to write the settings, to tell the player.
    void setCompletionHandler(std::function<void(const QuestSystem &)> onCompleted);

    /// Lets the quest progress, unless it is completed.
    void update(float elapsed) final;
};

} // namespace rtype::engine
