#pragma once

#include "rtype/ecs/System/System.hpp"

#include <string>
#include <vector>

namespace rtype::engine
{

/// A named event: triggering it updates every system subscribed to it.
class Event
{
    std::string _name;
    std::vector<ecs::System *> _subscribers;

public:
    explicit Event(std::string name);

    [[nodiscard]] const std::string &getName() const;

    /// The system must outlive the event, or be removed before.
    void addSubscriber(ecs::System &system);
    void removeSubscriber(ecs::System &system);

    /// Updates the enabled subscribers, in subscription order.
    void trigger();
};

} // namespace rtype::engine
