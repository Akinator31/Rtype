#include "Console/ConsoleSystem.hpp"

#include "Console/ConsoleScene.hpp"

namespace rtype::client
{

ConsoleSystem::ConsoleSystem(ecs::ComponentCaches & /*caches*/, engine::Engine &engine,
                             input::InputSource &input)
    : _engine(engine), _input(input)
{
}

bool ConsoleSystem::wasKeyPressed()
{
    const bool down = _input.isKeyDown(toggleKey);
    const bool pressed = down && !_keyWasDown;
    _keyWasDown = down;
    return pressed;
}

/// The console receives the input while it is open, so it is the scene at the top.
void ConsoleSystem::toggleConsole()
{
    if (_engine.isSceneOpen(ConsoleScene)) {
        _engine.popScene();
    } else {
        _engine.pushScene(ConsoleScene);
    }
}

void ConsoleSystem::update(float /*elapsed*/)
{
    if (wasKeyPressed()) {
        toggleConsole();
    }
}

} // namespace rtype::client
