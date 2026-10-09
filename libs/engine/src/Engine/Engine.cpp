#include "rtype/engine/Engine/Engine.hpp"

#include <algorithm>
#include <utility>

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

void Engine::subscribeToEvent(const std::string &name, rtype::ecs::System &system, Reaction reaction,
                              Duration duration)
{
    getEvent(name).addSubscriber(system, reaction, duration);
}

void Engine::triggerEvent(const std::string &name)
{
    auto it = _events.find(name);
    if (it != _events.end()) {
        const std::vector<Subscription> &subscriptions = it->second.getSubscriptions();
        _pendingReactions.insert(_pendingReactions.end(), subscriptions.begin(), subscriptions.end());
    }
}

void Engine::update(float elapsed)
{
    applyPendingReactions();
    _ecs.update(elapsed);
    restoreOnceReactions();
}

void Engine::applyPendingReactions()
{
    std::vector<Subscription> reactions = std::move(_pendingReactions);
    _pendingReactions.clear();

    for (const Subscription &reaction : reactions) {
        rtype::ecs::System &system = *reaction.system;
        auto restore = std::ranges::find(_restores, reaction.system, &Restore::system);
        if (reaction.duration == Duration::Permanent) {
            if (restore != _restores.end()) {
                _restores.erase(restore);
            }
        } else if (restore == _restores.end()) {
            _restores.push_back(Restore{.system = reaction.system, .enabled = system.enabled});
        }
        switch (reaction.reaction) {
        case Reaction::Enable:
            system.enabled = true;
            break;
        case Reaction::Disable:
            system.enabled = false;
            break;
        case Reaction::Toggle:
            system.enabled = !system.enabled;
            break;
        }
    }
}

void Engine::restoreOnceReactions()
{
    for (const Restore &restore : _restores) {
        restore.system->enabled = restore.enabled;
    }
    _restores.clear();
}

} // namespace rtype::engine
