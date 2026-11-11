#include "Commands/Commands.hpp"

#include "Commands/BossCommand.hpp"
#include "Commands/FpsCommand.hpp"
#include "Commands/HitboxesCommand.hpp"
#include "Commands/ProfileCommand.hpp"
#include "Commands/QuitCommand.hpp"
#include "Commands/SceneCommand.hpp"
#include "Commands/ToastCommand.hpp"

namespace rtype::client
{

void registerClientCommands(Console &console, engine::Engine &engine, render::Window &window,
                            GameScenes &games, const game::GameConfig &config, DebugViews &views)
{
    registerBossCommand(console, games, config);
    registerFpsCommand(console, views.fps);
    registerHitboxesCommand(console, views.hitboxes);
    registerProfileCommand(console, views.profile);
    registerQuitCommand(console, window);
    registerSceneCommand(console, engine);
    registerToastCommand(console, engine);
}

} // namespace rtype::client
