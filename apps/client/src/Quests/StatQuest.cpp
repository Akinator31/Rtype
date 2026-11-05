#include "Quests/StatQuest.hpp"

#include "Game/GameRecorder.hpp"

#include <utility>

namespace rtype::client
{

StatQuest::StatQuest(ecs::ComponentCaches & /*caches*/, std::string id, std::string event, const char *stat,
                     int target, engine::Settings &settings)
    : QuestSystem(std::move(id), {std::move(event)}, settings), _settings(settings), _stat(stat),
      _target(target)
{
}

bool StatQuest::progress()
{
    return getStat(_settings, _stat) >= _target;
}

} // namespace rtype::client
