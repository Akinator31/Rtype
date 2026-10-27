#pragma once

#include "ShipSelector/ShipSelection.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/render/ResourceManager.hpp"

namespace rtype::client
{

/// Name of the hangar scene, where the player picks a ship.
inline constexpr const char *ShipSelectorScene = "ship_selector";

/// Declares the hangar: the ships of the catalog, one at a time, turning above a pedestal on a starry sky,
/// with a carousel as in the design: arrows on each side, the name of the ship and a pagination under it,
/// its characteristics as gauges, and a button to equip it. Left and Right (ShipSelectorSystem) look at the
/// other ships. The scene opens on the ship that is equipped.
/// @param resources, style, translations, selection They must outlive the engine.
void createShipSelectorScene(engine::Engine &engine, render::ResourceManager &resources,
                             const engine::InterfaceStyle &style, localization::Localization &translations,
                             ShipSelection &selection);

} // namespace rtype::client
