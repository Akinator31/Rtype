#include "Commands/Commands.hpp"

#include "Commands/BossCommand.hpp"
#include "Commands/FpsCommand.hpp"
#include "Commands/HitboxesCommand.hpp"
#include "Commands/QuitCommand.hpp"
#include "Commands/SceneCommand.hpp"
#include "Commands/ToastCommand.hpp"
#include "Commands/VoiceCommand.hpp"

namespace rtype::client
{

void registerClientCommands(Console &console, engine::Engine &engine, render::Window &window,
                            GameScenes &games, const game::GameConfig &config, DebugViews &views,
                            VoiceDevices &voice)
{
    registerBossCommand(console, games, config);
    registerFpsCommand(console, views.fps);
    registerHitboxesCommand(console, views.hitboxes);
    registerQuitCommand(console, window);
    registerSceneCommand(console, engine);
    registerToastCommand(console, engine);
    registerVoiceCommand(console, voice);
}

} // namespace rtype::client
