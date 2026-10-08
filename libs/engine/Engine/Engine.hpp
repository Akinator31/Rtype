#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "Event/Event.hpp"

#include <string>
#include <unordered_map>

namespace rtype::engine
{

class Engine
{
    rtype::ecs::Ecs _ecs;
    std::unordered_map<std::string, Event> _events;

public:
    [[nodiscard]] rtype::ecs::Ecs &getEcs();

    /// Creates an event. If it already exists, returns it unchanged. The reference stays valid as
    /// long as the engine lives.
    Event &createEvent(const std::string &name);

    /// The event must exist (see hasEvent()), otherwise throws std::out_of_range.
    [[nodiscard]] Event &getEvent(const std::string &name);

    [[nodiscard]] bool hasEvent(const std::string &name) const;

    /// Subscribes a system to an event. The event must exist (see getEvent()).
    void subscribeToEvent(const std::string &name, rtype::ecs::System &system);

    /// Triggers an event. Does nothing if it does not exist.
    void triggerEvent(const std::string &name);
};

} // namespace rtype::engine
