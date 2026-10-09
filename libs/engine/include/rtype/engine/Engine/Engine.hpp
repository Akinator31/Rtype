#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Event/Event.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace rtype::engine
{

class Engine
{
    /// The state to give back to a system after a Duration::Once reaction.
    struct Restore {
        rtype::ecs::System *system;
        bool enabled;
    };

    rtype::ecs::Ecs _ecs;
    std::unordered_map<std::string, Event> _events;
    std::vector<Subscription> _pendingReactions;
    std::vector<Restore> _restores;

    void applyPendingReactions();
    void restoreOnceReactions();

public:
    [[nodiscard]] rtype::ecs::Ecs &getEcs();

    /// Creates an event. If it already exists, returns it unchanged. The reference stays valid as
    /// long as the engine lives.
    Event &createEvent(const std::string &name);

    /// The event must exist (see hasEvent()), otherwise throws std::out_of_range.
    [[nodiscard]] Event &getEvent(const std::string &name);

    [[nodiscard]] bool hasEvent(const std::string &name) const;

    /// Subscribes a system to an event. The event must exist (see getEvent()).
    void subscribeToEvent(const std::string &name, rtype::ecs::System &system, Reaction reaction,
                          Duration duration = Duration::Permanent);

    /// Triggers an event: the reactions of its current subscribers are applied at the start of the next
    /// update(), in the order of the triggers. Does nothing if the event does not exist.
    void triggerEvent(const std::string &name);

    /// Runs one frame: applies the pending reactions, updates the ECS, then gives back their previous state
    /// to the systems changed by a Duration::Once reaction. A permanent reaction cancels that restore.
    /// @param elapsed Time since the previous update, in seconds, given to every system.
    void update(float elapsed);
};

} // namespace rtype::engine
