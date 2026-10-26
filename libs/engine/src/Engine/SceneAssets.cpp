#include "rtype/engine/Engine/Engine.hpp"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

namespace rtype::engine
{

namespace
{

std::string errorMessage(std::exception_ptr error)
{
    try {
        std::rethrow_exception(error);
    } catch (const std::exception &problem) {
        return problem.what();
    } catch (...) {
        return "Unknown asset loading error";
    }
}

}

void Engine::setSceneAssetLoader(SceneAssetLoader &loader, const std::string &loadingScene)
{
    if (!hasScene(loadingScene) || !_sceneDefinitions.at(loadingScene).settings.assets.empty()) {
        throw std::invalid_argument("Loading scene must exist and have no assets");
    }
    if (_pendingScene || !_assetLeases.empty()) {
        throw std::logic_error("Scene assets are already in use");
    }
    _assetLoader = &loader;
    _loadingScene = loadingScene;
}

SceneLoadState Engine::sceneLoadState() const
{
    return _pendingScene ? _pendingScene->state : SceneLoadState::Idle;
}

const std::string &Engine::sceneLoadError() const
{
    static const std::string empty;
    return _pendingScene ? _pendingScene->error : empty;
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

void Engine::beginSceneRequest(const SceneRequest &request)
{
    std::vector<SceneAsset> assets = _sceneDefinitions.at(request.name).settings.assets;
    if (assets.empty()) {
        applyReadyScene(request, {});
        return;
    }
    if (_assetLoader == nullptr) {
        throw std::logic_error("Scene asset loader is not configured");
    }
    std::ranges::sort(assets);
    assets.erase(std::unique(assets.begin(), assets.end()), assets.end());

    PendingScene pending{.request = request};
    pending.assets.reserve(assets.size());
    try {
        acquireSceneAssets(pending, assets);
    } catch (...) {
        releaseAssets(pending.assets);
        collectUnusedAssets();
        pending.assets.clear();
        pending.state = SceneLoadState::Failed;
        pending.error = errorMessage(std::current_exception());
    }

    _pendingScene = std::move(pending);
    finishPendingScene();
    if (_pendingScene && !isSceneOpen(_loadingScene)) {
        openScene(_loadingScene);
    }
}

void Engine::acquireSceneAssets(PendingScene &pending, const std::vector<SceneAsset> &assets)
{
    for (const SceneAsset &asset : assets) {
        auto found = _assetLeases.find(asset);
        if (found == _assetLeases.end()) {
            found = _assetLeases.emplace(asset, AssetLease{_assetLoader->request(asset)}).first;
        }
        ++found->second.references;
        pending.assets.push_back(asset);
    }
}

void Engine::finishPendingScene()
{
    if (!_pendingScene || _pendingScene->state != SceneLoadState::Loading) {
        return;
    }
    for (const SceneAsset &asset : _pendingScene->assets) {
        if (!_assetLeases.at(asset).request.ready()) {
            return;
        }
    }
    try {
        for (const SceneAsset &asset : _pendingScene->assets) {
            _assetLeases.at(asset).request.complete();
        }
    } catch (...) {
        _pendingScene->error = errorMessage(std::current_exception());
        _pendingScene->state = SceneLoadState::Failed;
    }
    if (_pendingScene->state == SceneLoadState::Failed) {
        releaseAssets(_pendingScene->assets);
        _pendingScene->assets.clear();
        collectUnusedAssets();
        return;
    }

    SceneRequest request = std::move(_pendingScene->request);
    std::vector<SceneAsset> assets = std::move(_pendingScene->assets);
    _pendingScene.reset();
    closeLoadingScene();
    applyReadyScene(request, std::move(assets));
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
    releaseAssets(_pendingScene->assets);
    _pendingScene.reset();
    closeLoadingScene();
    collectUnusedAssets();
}

void Engine::closeLoadingScene()
{
    const auto loading = std::ranges::find(_namedScenes, _loadingScene, &NamedScene::name);
    if (loading != _namedScenes.end()) {
        _scenes.close(loading->id);
        destroyClosedScenes();
    }
}

void Engine::releaseAssets(const std::vector<SceneAsset> &assets)
{
    for (const SceneAsset &asset : assets) {
        auto found = _assetLeases.find(asset);
        if (found != _assetLeases.end()) {
            --found->second.references;
        }
    }
}

void Engine::collectUnusedAssets()
{
    for (auto it = _assetLeases.begin(); it != _assetLeases.end();) {
        if (it->second.references != 0 || !it->second.request.ready()) {
            ++it;
            continue;
        }
        try {
            it->second.request.complete();
        } catch (...) {
        }
        it->second.request.release();
        it = _assetLeases.erase(it);
    }
}

void Engine::unloadSceneAssets()
{
    if (_pendingScene) {
        cancelPendingScene();
    }
    _scenes.clear();
    destroyClosedScenes();
    collectUnusedAssets();
    _assetLeases.clear();
}

}
