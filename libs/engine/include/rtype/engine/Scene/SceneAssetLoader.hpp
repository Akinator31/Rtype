#pragma once

#include "rtype/engine/Scene/SceneAsset.hpp"
#include "rtype/engine/Scene/SceneAssetRequest.hpp"

namespace rtype::engine
{

/// Loads the assets of the scenes for the engine (see Engine::setSceneAssetLoader()), which does not know
/// how: the client loads them with its ResourceManager and its AudioPlayer.
class SceneAssetLoader
{
public:
    virtual ~SceneAssetLoader() = default;

    /// Starts loading an asset, without waiting for it. Called once per asset, until it is unloaded.
    virtual SceneAssetRequest requestAsset(const SceneAsset &asset) = 0;
};

} // namespace rtype::engine
