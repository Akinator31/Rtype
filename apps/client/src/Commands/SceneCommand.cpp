#include "Commands/SceneCommand.hpp"

#include <string>

namespace rtype::client
{

namespace
{

void switchScene(engine::Engine &engine, Console &console, const Console::Arguments &arguments)
{
    if (arguments.size() != 1) {
        console.print("Usage: scene <name>");
    } else if (!engine.hasScene(arguments[0])) {
        console.print("Unknown scene: " + arguments[0]);
    } else {
        engine.switchScene(arguments[0]);
    }
}

} // namespace

void registerSceneCommand(Console &console, engine::Engine &engine)
{
    console.registerConsoleCommand("scene", "scene <name>: closes every scene and opens this one",
                                   [&engine](Console &target, const Console::Arguments &arguments) {
                                       switchScene(engine, target, arguments);
                                   });
}

} // namespace rtype::client
