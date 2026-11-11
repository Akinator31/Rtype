#pragma once

#include "Console/Console.hpp"
#include "Game/GameScenes.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/game/Config/GameConfig.hpp"
#include "rtype/render/Window.hpp"

namespace rtype::client
{

/// The views for debugging that the console shows and hides, read by the game loop.
struct DebugViews {
    /// The hitboxes of the entities and the collision grid ("hitboxes", F3).
    bool hitboxes = false;
    /// The number of frames per second ("fps").
    bool fps = false;
    /// The diagnostics of the frames, in the log ("profile", or RTYPE_PROFILE_FRAMES=1).
    bool profile = false;
};

/// Registers every console command of the client. Each command has its own file in Commands/: to add one,
/// write its register function there and call it from here.
/// @param engine, window, games, config, views They must outlive the console.
/// @param views The views switched by the "hitboxes", "fps" and "profile" commands.
void registerClientCommands(Console &console, engine::Engine &engine, render::Window &window,
                            GameScenes &games, const game::GameConfig &config, DebugViews &views);

} // namespace rtype::client
