#pragma once

#include "Config/TableReader.hpp"
#include "rtype/game/Enemy/EnemyDefinition.hpp"
#include "rtype/game/Item/ItemDefinition.hpp"
#include "rtype/game/Loadout.hpp"
#include "rtype/game/Player/PlayerDefinition.hpp"
#include "rtype/game/Progression.hpp"
#include "rtype/game/Projectile/ProjectileDefinition.hpp"
#include "rtype/game/Ship/ShipDefinition.hpp"
#include "rtype/game/ShipUnlock.hpp"
#include "rtype/game/Wave/WaveDefinition.hpp"
#include "rtype/game/Weapon/WeaponDefinition.hpp"

#include <vector>

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
/// What config/hangar.lua holds: the levels of its ships and of its weapons.
struct Hangar {
    std::vector<ShipUnlock> ships;
    std::vector<WeaponUnlock> weapons;
};

/// The `ships` and `weapons` lists of config/hangar.lua; a ship or a weapon listed twice is an error. The ids
/// of the weapons are checked against the weapons of the configuration afterwards.
[[nodiscard]] Hangar readHangar(TableReader &table);

} // namespace rtype::game
