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
    std::vector<rtype::ecs::System *> _subscribers;

public:
    explicit Event(std::string name);

    [[nodiscard]] const std::string &getName() const;

    /// The system must outlive the event, or be removed before.
    void addSubscriber(rtype::ecs::System &system);
    void removeSubscriber(rtype::ecs::System &system);

    /// Updates the enabled subscribers, in subscription order, with an elapsed time of 0.
    void trigger();
};

} // namespace rtype::engine
