#include "Commands/Commands.hpp"

#include "Commands/BossCommand.hpp"
#include "Commands/HitboxesCommand.hpp"
#include "Commands/QuitCommand.hpp"
#include "Commands/SceneCommand.hpp"
#include "Commands/ToastCommand.hpp"

namespace rtype::client
{

void registerClientCommands(Console &console, engine::Engine &engine, render::Window &window,
                            GameScenes &games, const game::GameConfig &config, bool &showHitboxes)
{
    registerBossCommand(console, games, config);
    registerHitboxesCommand(console, showHitboxes);
    registerQuitCommand(console, window);
    registerSceneCommand(console, engine);
    registerToastCommand(console, engine);
}

} // namespace rtype::client
