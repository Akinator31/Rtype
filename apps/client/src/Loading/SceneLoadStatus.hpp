#pragma once

#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/input/Devices.hpp"
#include "rtype/render/Renderer.hpp"

namespace rtype::client
{

/// Tells the player, over every scene, that a scene waits for its assets (Engine::getSceneLoadState()), or
/// that one of them could not load: then R loads them again.
class SceneLoadStatus
{
    engine::Engine &_engine;
    const input::Devices &_devices;
    bool _retryKeyWasDown = false;

public:
    /// @param engine, devices They must outlive the status.
    SceneLoadStatus(engine::Engine &engine, const input::Devices &devices);

    /// Retries the load when R is pressed after a failure. Call it once per frame.
    void update();
    /// Draws the state of the load, if a scene waits for its assets.
    void draw(render::Renderer &renderer) const;
};

} // namespace rtype::client
