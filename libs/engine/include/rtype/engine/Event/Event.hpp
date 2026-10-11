#pragma once

#include "rtype/ecs/System/System.hpp"

#include <string>
#include <vector>

namespace rtype::engine
{

/// What triggering an event does to the `enabled` flag of a subscribed system (ADR 0008).
enum class Reaction { Enable, Disable, Toggle };

/// How long a reaction lasts: for good, or for the next engine update only.
enum class Duration { Permanent, Once };

struct Subscription {
    rtype::ecs::System *system;
    Reaction reaction;
    Duration duration;
};

/// A named event: triggering it (see Engine::triggerEvent()) changes the `enabled` flag of its subscribed
/// systems at the next engine update. It never runs them itself.
class Event
{
    std::string _name;
    std::vector<Subscription> _subscriptions;

public:
    explicit Event(std::string name);

    [[nodiscard]] const std::string &getName() const;

    /// The system must outlive the event, or be removed before.
    void addSubscriber(rtype::ecs::System &system, Reaction reaction,
                       Duration duration = Duration::Permanent);
    /// Removes every subscription of the system.
    void removeSubscriber(rtype::ecs::System &system);

    /// In subscription order.
    [[nodiscard]] const std::vector<Subscription> &getSubscriptions() const;
};

} // namespace rtype::engine
