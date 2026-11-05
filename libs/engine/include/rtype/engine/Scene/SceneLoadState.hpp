#pragma once

namespace rtype::engine
{

/// Where the scene waiting for its assets is (see Engine::getSceneLoadState()).
enum class SceneLoadState {
    /// No scene is waiting for its assets.
    Idle,
    /// A scene waits for its assets: the loading scene is shown.
    Loading,
    /// An asset of the waiting scene could not load: see Engine::retrySceneLoad().
    Failed,
};

} // namespace rtype::engine
