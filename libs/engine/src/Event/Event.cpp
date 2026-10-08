#include "rtype/engine/Event/Event.hpp"

#include <algorithm>
#include <utility>

namespace rtype::engine
{

Event::Event(std::string name) : _name(std::move(name)) {}

const std::string &Event::getName() const
{
    return _name;
}

void Event::addSubscriber(rtype::ecs::System &system)
{
    _subscribers.push_back(&system);
}

void Event::removeSubscriber(rtype::ecs::System &system)
{
    std::erase(_subscribers, &system);
}

void Event::trigger()
{
    for (rtype::ecs::System *system : _subscribers) {
        if (system->enabled) {
            system->update();
        }
    }
}

} // namespace rtype::engine
