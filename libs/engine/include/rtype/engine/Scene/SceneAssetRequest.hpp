#pragma once

#include <functional>

namespace rtype::engine
{

/// An asset requested from a SceneAssetLoader: how the engine follows its load, then unloads it.
struct SceneAssetRequest {
    /// Tells whether the load is over, successfully or not. Never blocks.
    std::function<bool()> isReady;
    /// Throws the error of the load if it failed. Only called once the asset is ready.
    std::function<void()> checkLoaded;
    /// Unloads the asset. Only called once the asset is ready, and no scene uses it anymore.
    std::function<void()> unload;
};

} // namespace rtype::engine
