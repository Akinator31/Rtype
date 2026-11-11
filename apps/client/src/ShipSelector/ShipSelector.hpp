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
/// looked at and plays its take-off into the sky of the game, then calls it; the ship flies on until the game
/// opens, on the shot the hangar shows, without a cut. The game scene loads meanwhile, from the opening of
/// the hangar (Engine::preloadScene()), so that it opens without the loading scene. Empty, the hangar only
/// equips ships.
void openShipSelector(engine::Engine &engine, ShipSelection &selection, std::function<void()> launch = {});

} // namespace rtype::client
