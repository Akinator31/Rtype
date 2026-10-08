#include "rtype/engine/Engine/Engine.hpp"

namespace rtype::engine
{

rtype::ecs::Ecs &Engine::getEcs()
{
    return _ecs;
}

Event &Engine::createEvent(const std::string &name)
{
    return _events.try_emplace(name, name).first->second;
}

Event &Engine::getEvent(const std::string &name)
{
    return _events.at(name);
}

bool Engine::hasEvent(const std::string &name) const
{
    return _events.contains(name);
}

void Engine::subscribeToEvent(const std::string &name, rtype::ecs::System &system)
{
    getEvent(name).addSubscriber(system);
}

void Engine::triggerEvent(const std::string &name)
{
    auto it = _events.find(name);
    if (it != _events.end()) {
        it->second.trigger();
    }
}

} // namespace rtype::engine
