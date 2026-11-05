#pragma once

#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/engine/Quest/QuestSystem.hpp"
#include "rtype/engine/Settings/Settings.hpp"

#include <string>

namespace rtype::client
{

/// A quest completed once a stat of the player (GameRecorder.hpp) reaches a target: shoot down 50 Bydos,
/// reach the wave 3... Woken by the event that counts the stat.
class StatQuest : public engine::QuestSystem
{
    const engine::Settings &_settings;
    const char *_stat;
    int _target;

    bool progress() override;

public:
    StatQuest(ecs::ComponentCaches &caches, std::string id, std::string event, const char *stat, int target,
              engine::Settings &settings);
};

} // namespace rtype::client
