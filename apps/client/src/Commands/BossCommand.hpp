#pragma once

#include "Console/Console.hpp"
#include "Game/GameScenes.hpp"
#include "rtype/game/Config/GameConfig.hpp"

namespace rtype::client
{

/// "boss": starts a solo game that goes straight to the final boss, without the other waves.
/// @param games, config They must outlive the console.
void registerBossCommand(Console &console, GameScenes &games, const game::GameConfig &config);

} // namespace rtype::client
