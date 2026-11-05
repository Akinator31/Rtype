#pragma once

#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/engine/Quest/QuestSystem.hpp"
#include "rtype/engine/Settings/Settings.hpp"

#include <string>

namespace rtype::client
{

/// A quest completed the first time its event is triggered: shoot down a Bydo, a boss...
class EventQuest : public engine::QuestSystem
{
    bool progress() override;

public:
    EventQuest(ecs::ComponentCaches &caches, std::string id, std::string event, engine::Settings &settings);
};

} // namespace rtype::client
