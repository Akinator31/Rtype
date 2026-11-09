#pragma once

#include "Breaker.hpp"
#include "Looks.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Style.hpp"

#include <functional>

namespace orbit
{

/// The scenes of the game: the menu, the arena, the pause over it and the end of a game over it.
inline constexpr const char *MenuScene = "menu";
inline constexpr const char *GameScene = "game";
inline constexpr const char *PauseScene = "pause";
inline constexpr const char *GameOverScene = "game_over";

/// Where the best score is kept in the settings of the engine.
inline constexpr const char *BestScoreKey = "orbit_breaker.best";

/// Declares the scenes, all under a sky of space: the menu, with its title and borders of bricks shimmering;
/// the arena, with its bar of score and lives; the pause and the end of a game, over the dimmed arena. A game
/// starts with a new Match, keeping the best score; its end writes the best score to the settings of the
/// engine.
/// @param style, looks, match They must outlive the engine.
/// @param quit Closes the game, from the menu.
void createScenes(rtype::engine::Engine &engine, const rtype::engine::InterfaceStyle &style,
                  const Looks &looks, Match &match, std::function<void()> quit);

} // namespace orbit
