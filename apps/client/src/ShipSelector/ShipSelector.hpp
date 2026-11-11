#pragma once

#include "ShipSelector/ShipSelection.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/render/ResourceManager.hpp"

#include <functional>

namespace rtype::client
{

/// Name of the hangar scene, where the player picks a ship.
inline constexpr const char *ShipSelectorScene = "ship_selector";

/// Declares the hangar: the ships of the catalog, one at a time, turning above a pedestal on a starry sky,
/// with a carousel as in the design: arrows on each side, the name of the ship and a pagination under it,
/// its characteristics as gauges, and a button to equip it. Left and Right (ShipSelectorSystem) look at the
/// other ships. The scene opens on the ship that is equipped. The ship equipped is kept in the settings of
/// the engine. Opened before a game (openShipSelector() with `launch`), its button launches the game with
/// the ship looked at.
/// @param resources, style, translations, selection They must outlive the engine.
void createShipSelectorScene(engine::Engine &engine, render::ResourceManager &resources,
                             const engine::InterfaceStyle &style, localization::Localization &translations,
                             ShipSelection &selection);

/// Opens the hangar over the open scenes.
/// @param launch Starts the game the player picks a ship for: the button of the hangar then equips the ship
/// looked at, closes the hangar and calls it. Empty, the hangar only equips ships.
/// @param allUnlocked Every ship is open, whatever the level of the account: for a game offline. Otherwise
/// the ships above the level are locked again (ShipSelection::setAllUnlocked()).
void openShipSelector(engine::Engine &engine, ShipSelection &selection, std::function<void()> launch = {},
                      bool allUnlocked = false);

} // namespace rtype::client
