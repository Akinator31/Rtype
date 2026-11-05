#include "Loading/SceneLoadStatus.hpp"

namespace rtype::client
{

namespace
{

constexpr render::Vector2 StatusPosition{80.0f, 90.0f};
constexpr float LoadingTextSize = 40.0f;
constexpr float ErrorTextSize = 30.0f;

} // namespace

SceneLoadStatus::SceneLoadStatus(engine::Engine &engine, const input::Devices &devices)
    : _engine(engine), _devices(devices)
{
}

void SceneLoadStatus::update()
{
    const bool retryKeyDown = _devices.isKeyDown(input::Key::R);
    if (retryKeyDown && !_retryKeyWasDown) {
        _engine.retrySceneLoad();
    }
    _retryKeyWasDown = retryKeyDown;
}

void SceneLoadStatus::draw(render::Renderer &renderer) const
{
    const engine::SceneLoadState state = _engine.getSceneLoadState();
    if (state == engine::SceneLoadState::Loading) {
        renderer.drawText("Loading scene...", StatusPosition, LoadingTextSize, render::colors::White);
    } else if (state == engine::SceneLoadState::Failed) {
        renderer.drawText("Asset load failed: " + _engine.getSceneLoadError() + "  Press R to retry",
                          StatusPosition, ErrorTextSize, render::colors::White);
    }
}

} // namespace rtype::client
