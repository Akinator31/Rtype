#pragma once

#include "rtype/game/Enemy/EnemyDefinition.hpp"
#include "rtype/game/Player/PlayerDefinition.hpp"
#include "rtype/game/Projectile/ProjectileDefinition.hpp"
#include "rtype/game/Ship/ShipDefinition.hpp"
#include "rtype/game/Wave/WaveDefinition.hpp"
#include "rtype/game/Weapon/WeaponDefinition.hpp"

#include <vector>

namespace rtype::game
{

/// The definitions of a game: what loadGameConfig() reads from the configuration files (ADR 0018), given to
/// each Simulation, which registers them in its GameContext. Plain values, so that the files are read once
/// for every game.
struct GameConfig {
    std::vector<ProjectileDefinition> projectiles;
    std::vector<WeaponDefinition> weapons;
    std::vector<ShipDefinition> ships;
    std::vector<EnemyDefinition> enemies;
    std::vector<WaveDefinition> waves;
    PlayerDefinition player;
};

} // namespace rtype::game
