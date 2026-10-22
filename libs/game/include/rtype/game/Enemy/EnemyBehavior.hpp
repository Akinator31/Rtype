#pragma once

#include <string>

namespace rtype::game
{

/// The behavior of a kind of enemy: Lua code that the EnemySystem runs for each of its enemies at every tick,
/// to move them and to make them fire (docs/architecture/game.md, "Enemy behaviors").
struct EnemyBehavior {
    /// What its errors call it, e.g. "enemies/bydo_gunner.lua".
    std::string file;
    /// A Lua chunk that returns a table with an `update(enemy, game)` function: the file of the enemy.
    std::string code;
};

} // namespace rtype::game
