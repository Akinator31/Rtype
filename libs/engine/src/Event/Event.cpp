#include "rtype/engine/Event/Event.hpp"

#include <utility>

namespace rtype::engine
{

Event::Event(std::string name) : _name(std::move(name)) {}

const std::string &Event::getName() const
{
    return _name;
}

void Event::addSubscriber(rtype::ecs::System &system, Reaction reaction, Duration duration)
{
    _subscriptions.push_back(Subscription{.system = &system, .reaction = reaction, .duration = duration});
}

void Event::removeSubscriber(rtype::ecs::System &system)
{
    std::erase_if(_subscriptions,
                  [&system](const Subscription &subscription) { return subscription.system == &system; });
}

const std::vector<Subscription> &Event::getSubscriptions() const
{
    return _subscriptions;
}

} // namespace rtype::engine
