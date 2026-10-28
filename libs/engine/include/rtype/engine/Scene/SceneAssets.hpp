#pragma once

#include "rtype/engine/Scene/SceneAsset.hpp"
#include "rtype/engine/Scene/SceneAssetLoader.hpp"
#include "rtype/engine/Scene/SceneAssetRequest.hpp"

#include <cstddef>
#include <map>
#include <vector>

namespace rtype::engine
{

/// The assets of the scenes, shared between them: an asset is requested from the SceneAssetLoader when a
/// first scene acquires it, and unloaded once every scene that acquired it has released it.
///
/// The Engine acquires the assets of a scene before opening it, waits until they are ready, checks that they
/// loaded, and releases them when the scene closes. A released asset is only unloaded by
/// unloadUnusedAssets(), once its load is over.
class SceneAssets
{
    struct Lease {
        SceneAssetRequest request;
        /// How many times the asset is acquired and not released yet.
        std::size_t users = 0;
    };

    SceneAssetLoader *_loader = nullptr;
    std::map<SceneAsset, Lease> _leases;

    void acquireAsset(const SceneAsset &asset);

public:
    /// Sets the loader of the assets. It must outlive the assets it loaded.
    /// @throws std::logic_error if assets are still acquired or loading.
    void setLoader(SceneAssetLoader &loader);
    [[nodiscard]] bool hasLoader() const;

    /// Acquires each asset, requesting it from the loader unless another scene already acquired it. The
    /// loader must be set.
    /// @throws Anything thrown by the loader. No asset is acquired then.
    void acquireAssets(const std::vector<SceneAsset> &assets);
    /// Tells whether the loads of these acquired assets are all over, successfully or not.
    [[nodiscard]] bool areAssetsReady(const std::vector<SceneAsset> &assets) const;
    /// Checks that these acquired assets, all ready, loaded.
    /// @throws Anything thrown by the loader for the first asset that failed.
    void checkAssetsLoaded(const std::vector<SceneAsset> &assets) const;
    /// Releases these acquired assets. They stay loaded until unloadUnusedAssets().
    void releaseAssets(const std::vector<SceneAsset> &assets);
    /// Unloads the assets that no scene uses anymore, once their load is over.
    void unloadUnusedAssets();
    /// Forgets every asset without unloading it: at shutdown, for the assets still loading.
    void forgetAssets();
};

} // namespace rtype::engine
