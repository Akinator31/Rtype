#include "Commands/Commands.hpp"

#include "Commands/QuitCommand.hpp"
#include "Commands/SceneCommand.hpp"
#include "Commands/ToastCommand.hpp"

namespace rtype::client
{

void registerClientCommands(Console &console, engine::Engine &engine, render::Window &window)
{
    registerQuitCommand(console, window);
    registerSceneCommand(console, engine);
    registerToastCommand(console, engine);
}

} // namespace rtype::client
