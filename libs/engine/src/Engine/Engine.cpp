#include "rtype/engine/Engine/Engine.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace rtype::engine
{

Engine::Engine() : _scenes(_ecs.registerSystem<Scenes>()) {}

rtype::ecs::Ecs &Engine::getEcs()
{
    return _ecs;
}

Scenes &Engine::getScenes()
{
    return _scenes;
}

Controls &Engine::getControls()
{
    return _controls;
}

void Engine::setInputSource(input::InputSource &source)
{
    _inputSource = &source;
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

void Engine::createScene(const std::string &name, SceneSettings settings,
                         std::function<void(Engine &, SceneId)> build)
{
    if (!_sceneDefinitions.try_emplace(name, std::move(settings), std::move(build)).second) {
        return;
    }
    createEvent(name + ":open");
    createEvent(name + ":close");
}

bool Engine::hasScene(const std::string &name) const
{
    return _sceneDefinitions.contains(name);
}

bool Engine::isSceneOpen(const std::string &name) const
{
    return std::ranges::find(_namedScenes, name, &NamedScene::name) != _namedScenes.end();
}

void Engine::switchScene(const std::string &name)
{
    if (!hasScene(name)) {
        throw std::out_of_range("Unknown scene: " + name);
    }
    _sceneRequests.push_back({SceneChange::Switch, name});
}

void Engine::pushScene(const std::string &name)
{
    if (!hasScene(name)) {
        throw std::out_of_range("Unknown scene: " + name);
    }
    _sceneRequests.push_back({SceneChange::Push, name});
}

void Engine::popScene()
{
    _sceneRequests.push_back({SceneChange::Pop, ""});
}

ecs::entity_t Engine::showToast(const std::string &text, ToastType type)
{
    // No SceneMember: the toast outlives the scene that shows it
    const ecs::entity_t toast = _ecs.createEntity();
    _ecs.addComponent<Toast>(toast, text, type);
    return toast;
}

void Engine::update(float elapsed)
{
    if (_inputSource != nullptr) {
        _controls.update(*_inputSource, elapsed);
    }
    applySceneRequests();
    applyPendingReactions();
    _ecs.update(elapsed);
    restoreOnceReactions();
}

void Engine::applySceneRequests()
{
    destroyClosedScenes();

    std::vector<SceneRequest> requests = std::move(_sceneRequests);
    _sceneRequests.clear();

    for (const SceneRequest &request : requests) {
        switch (request.change) {
        case SceneChange::Switch:
            _scenes.clear();
            destroyClosedScenes();
            openScene(request.name);
            break;
        case SceneChange::Push:
            openScene(request.name);
            break;
        case SceneChange::Pop:
            _scenes.close();
            destroyClosedScenes();
            break;
        }
    }
}

void Engine::openScene(const std::string &name)
{
    const SceneDefinition &definition = _sceneDefinitions.at(name);
    const SceneId id = _scenes.open(definition.settings);
    _namedScenes.push_back({id, name});
    if (definition.build) {
        definition.build(*this, id);
    }
    triggerEvent(name + ":open");
}

void Engine::destroyClosedScenes()
{
    rtype::ecs::ComponentCache<SceneMember> &members = _ecs.cache<SceneMember>();
    for (const SceneId scene : _scenes.takeClosedScenes()) {
        for (rtype::ecs::entity_t entity = 0; entity < members.size(); ++entity) {
            if (members.has(entity) && members.get(entity).scene == scene) {
                _ecs.destroyEntity(entity);
            }
        }
        const auto named = std::ranges::find(_namedScenes, scene, &NamedScene::id);
        if (named != _namedScenes.end()) {
            triggerEvent(named->name + ":close");
            _namedScenes.erase(named);
        }
    }
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
