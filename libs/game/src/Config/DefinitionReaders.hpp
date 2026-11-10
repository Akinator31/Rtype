#pragma once

#include "Config/TableReader.hpp"
#include "rtype/game/Enemy/EnemyDefinition.hpp"
#include "rtype/game/Item/ItemDefinition.hpp"
#include "rtype/game/Player/PlayerDefinition.hpp"
#include "rtype/game/Progression.hpp"
#include "rtype/game/Projectile/ProjectileDefinition.hpp"
#include "rtype/game/Ship/ShipDefinition.hpp"
#include "rtype/game/ShipUnlock.hpp"
#include "rtype/game/Wave/WaveDefinition.hpp"
#include "rtype/game/Weapon/WeaponDefinition.hpp"

/// The table a configuration file returns, read into the definition of its kind. The fields have the names
/// of the members of the definition; docs/guides/configure-enemies-and-weapons.md lists them. Each one throws
/// a ConfigError for a field that is missing, of the wrong type, out of range or unknown.
namespace rtype::game
{

[[nodiscard]] ProjectileDefinition readProjectileDefinition(TableReader &table);
[[nodiscard]] WeaponDefinition readWeaponDefinition(TableReader &table);
[[nodiscard]] ShipDefinition readShipDefinition(TableReader &table);
[[nodiscard]] EnemyDefinition readEnemyDefinition(TableReader &table);
[[nodiscard]] WaveDefinition readWaveDefinition(TableReader &table);
[[nodiscard]] ItemDefinition readItemDefinition(TableReader &table);
[[nodiscard]] PlayerDefinition readPlayerDefinition(TableReader &table);
[[nodiscard]] Progression readProgression(TableReader &table);
/// The `ships` list of config/hangar.lua; a ship listed twice is an error.
[[nodiscard]] std::vector<ShipUnlock> readShipUnlocks(TableReader &table);

} // namespace rtype::game
