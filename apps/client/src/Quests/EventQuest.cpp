#include "Quests/EventQuest.hpp"

#include <utility>

namespace rtype::client
{

EventQuest::EventQuest(ecs::ComponentCaches & /*caches*/, std::string id, std::string event,
                       engine::Settings &settings)
    : QuestSystem(std::move(id), {std::move(event)}, settings)
{
}

bool EventQuest::progress()
{
    return true;
}

} // namespace rtype::client
