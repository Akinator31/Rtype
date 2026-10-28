#include "rtype/engine/Engine/Engine.hpp"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

namespace rtype::engine
{

namespace
{

std::string getErrorMessage(const std::exception_ptr &error)
{
    try {
        std::rethrow_exception(error);
    } catch (const std::exception &problem) {
        return problem.what();
    } catch (...) {
        return "Unknown asset loading error";
    }
}

} // namespace

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

Settings &Engine::getSettings()
{
    return _settings;
}

void Engine::setSceneAssetLoader(SceneAssetLoader &loader, const std::string &loadingScene)
{
    if (!hasScene(loadingScene) || !_sceneDefinitions.at(loadingScene).settings.assets.empty()) {
        throw std::invalid_argument("Loading scene must exist and have no assets");
    }
    if (_pendingScene) {
        throw std::logic_error("Scene assets are already in use");
    }
    _sceneAssets.setLoader(loader);
    _loadingScene = loadingScene;
}

SceneLoadState Engine::getSceneLoadState() const
{
    return _pendingScene ? _pendingScene->state : SceneLoadState::Idle;
}

const std::string &Engine::getSceneLoadError() const
{
    static const std::string noError;
    return _pendingScene ? _pendingScene->error : noError;
}

void Engine::retrySceneLoad()
{
    if (!_pendingScene || _pendingScene->state != SceneLoadState::Failed) {
        return;
    }
    const SceneRequest request = _pendingScene->request;
    _pendingScene.reset();
    beginSceneRequest(request);
}

void Engine::unloadSceneAssets()
{
    if (_pendingScene) {
        cancelPendingScene();
    }
    _scenes.clear();
    destroyClosedScenes();
    _sceneAssets.forgetAssets();
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

ecs::entity_t Engine::showToast(const std::string &subject, const std::string &message,
                                const std::string &actor, ToastType type)
{
    const ecs::entity_t toast = _ecs.createEntity();
    _ecs.addComponent<Toast>(toast, message, type, subject, actor);
    return toast;
}

ecs::entity_t Engine::showAnnouncement(const std::string &title, const std::string &detail, ToastType type)
{
    const ecs::entity_t toast = _ecs.createEntity();
    _ecs.addComponent<Toast>(toast, detail, type, title, std::string(), ToastKind::Announcement);
    return toast;
}

ecs::entity_t Engine::showBanner(const std::string &title, const std::string &detail, ToastType type)
{
    const ecs::entity_t toast = _ecs.createEntity();
    _ecs.addComponent<Toast>(toast, detail, type, title, std::string(), ToastKind::Banner);
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

    if (requests.empty()) {
        finishPendingScene();
    }
    for (const SceneRequest &request : requests) {
        if (_pendingScene) {
            cancelPendingScene();
        }
        switch (request.change) {
        case SceneChange::Switch:
        case SceneChange::Push:
            beginSceneRequest(request);
            break;
        case SceneChange::Pop:
            _scenes.close();
            destroyClosedScenes();
            break;
        }
    }
}

void Engine::openScene(const std::string &name, std::vector<SceneAsset> assets)
{
    const SceneDefinition &definition = _sceneDefinitions.at(name);
    const SceneId id = _scenes.open(definition.settings);
    _namedScenes.push_back({id, name, std::move(assets)});
    try {
        if (definition.build) {
            definition.build(*this, id);
        }
    } catch (...) {
        _scenes.close(id);
        destroyClosedScenes();
        throw;
    }
    triggerEvent(name + ":open");
}

/// Opens the scene at once if it has no assets or if they are all loaded already. Otherwise, it waits for
/// them under the loading scene, and finishPendingScene() opens it.
void Engine::beginSceneRequest(const SceneRequest &request)
{
    const std::vector<SceneAsset> &assets = _sceneDefinitions.at(request.name).settings.assets;
    if (assets.empty()) {
        applyReadyScene(request, {});
        return;
    }
    if (!_sceneAssets.hasLoader()) {
        throw std::logic_error("Scene asset loader is not configured");
    }
    PendingScene pending{.request = request};
    try {
        _sceneAssets.acquireAssets(assets);
        pending.assets = assets;
    } catch (...) {
        pending.state = SceneLoadState::Failed;
        pending.error = getErrorMessage(std::current_exception());
    }
    _pendingScene = std::move(pending);
    finishPendingScene();
    if (_pendingScene && !isSceneOpen(_loadingScene)) {
        openScene(_loadingScene);
    }
}

void Engine::finishPendingScene()
{
    if (!_pendingScene || _pendingScene->state != SceneLoadState::Loading ||
        !_sceneAssets.areAssetsReady(_pendingScene->assets)) {
        return;
    }
    try {
        _sceneAssets.checkAssetsLoaded(_pendingScene->assets);
    } catch (...) {
        failPendingScene(getErrorMessage(std::current_exception()));
        return;
    }
    PendingScene ready = std::move(*_pendingScene);
    _pendingScene.reset();
    closeLoadingScene();
    applyReadyScene(ready.request, std::move(ready.assets));
}

/// The waiting scene keeps its request, to be retried, but not its assets.
void Engine::failPendingScene(std::string error)
{
    _sceneAssets.releaseAssets(_pendingScene->assets);
    _sceneAssets.unloadUnusedAssets();
    _pendingScene->assets.clear();
    _pendingScene->state = SceneLoadState::Failed;
    _pendingScene->error = std::move(error);
}

void Engine::applyReadyScene(const SceneRequest &request, std::vector<SceneAsset> assets)
{
    if (request.change == SceneChange::Switch) {
        _scenes.clear();
        destroyClosedScenes();
    }
    openScene(request.name, std::move(assets));
}

void Engine::cancelPendingScene()
{
    _sceneAssets.releaseAssets(_pendingScene->assets);
    _pendingScene.reset();
    closeLoadingScene();
    _sceneAssets.unloadUnusedAssets();
}

void Engine::closeLoadingScene()
{
    const auto loading = std::ranges::find(_namedScenes, _loadingScene, &NamedScene::name);
    if (loading != _namedScenes.end()) {
        _scenes.close(loading->id);
        destroyClosedScenes();
    }
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
            _sceneAssets.releaseAssets(named->assets);
            _namedScenes.erase(named);
        }
    }
    _sceneAssets.unloadUnusedAssets();
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
