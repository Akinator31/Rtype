#include "rtype/engine/Scene/SceneAssets.hpp"

#include <stdexcept>

namespace rtype::engine
{

void SceneAssets::setLoader(SceneAssetLoader &loader)
{
    if (!_leases.empty()) {
        throw std::logic_error("Scene assets are already in use");
    }
    _loader = &loader;
}

bool SceneAssets::hasLoader() const
{
    return _loader != nullptr;
}

void SceneAssets::acquireAsset(const SceneAsset &asset)
{
    auto lease = _leases.find(asset);
    if (lease == _leases.end()) {
        lease = _leases.emplace(asset, Lease{_loader->requestAsset(asset)}).first;
    }
    ++lease->second.users;
}

void SceneAssets::acquireAssets(const std::vector<SceneAsset> &assets)
{
    for (auto asset = assets.begin(); asset != assets.end(); ++asset) {
        try {
            acquireAsset(*asset);
        } catch (...) {
            releaseAssets({assets.begin(), asset});
            throw;
        }
    }
}

bool SceneAssets::areAssetsReady(const std::vector<SceneAsset> &assets) const
{
    for (const SceneAsset &asset : assets) {
        if (!_leases.at(asset).request.isReady()) {
            return false;
        }
    }
    return true;
}

void SceneAssets::checkAssetsLoaded(const std::vector<SceneAsset> &assets) const
{
    for (const SceneAsset &asset : assets) {
        _leases.at(asset).request.checkLoaded();
    }
}

void SceneAssets::releaseAssets(const std::vector<SceneAsset> &assets)
{
    for (const SceneAsset &asset : assets) {
        if (const auto lease = _leases.find(asset); lease != _leases.end()) {
            --lease->second.users;
        }
    }
}

void SceneAssets::unloadUnusedAssets()
{
    for (auto lease = _leases.begin(); lease != _leases.end();) {
        if (lease->second.users != 0 || !lease->second.request.isReady()) {
            ++lease;
            continue;
        }
        lease->second.request.unload();
        lease = _leases.erase(lease);
    }
}

void SceneAssets::forgetAssets()
{
    _leases.clear();
}

} // namespace rtype::engine
