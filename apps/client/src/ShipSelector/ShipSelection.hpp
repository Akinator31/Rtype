#pragma once

#include "ShipSelector/ShipCatalog.hpp"
#include "rtype/engine/Settings/Settings.hpp"

#include <cstddef>
#include <functional>
#include <string_view>

namespace rtype::client
{

/// What the player has chosen in the hangar: the ship looked at, and the one equipped, as indices in the
/// catalog.
struct ShipSelection {
    std::size_t focused = 0;
    std::size_t equipped = 0;
    /// Where the carousel is, as an index of the catalog that glides towards `focused`, around: 2.5 is
    /// halfway between the third and the fourth ship. Moved by the ShipSelectorSystem.
    float slide = 0.0f;
    /// What to do once the player has picked the ship of the game it starts: the hangar then launches the
    /// game instead of only equipping the ship. Empty when the hangar is opened to look at the ships.
    std::function<void()> launch;

    /// Looks at the next ship (+1) or the previous one (-1), around.
    void step(int direction);
    /// Equips the ship looked at, unless it is locked.
    void equipFocused();
    [[nodiscard]] bool isFocusedEquipped() const { return focused == equipped; }
    [[nodiscard]] const ShipEntry &getFocused() const { return getShipCatalog()[focused]; }

    /// The ship the carousel is nearest to: what the texts and the gauges show while it glides.
    [[nodiscard]] std::size_t getShownIndex() const;
    [[nodiscard]] const ShipEntry &getShown() const { return getShipCatalog()[getShownIndex()]; }
    /// 1 when the carousel rests on a ship, 0 halfway between two: the texts fade out and in while it glides.
    [[nodiscard]] float getShownClarity() const;
    /// A characteristic mixed between the two ships the carousel is between.
    [[nodiscard]] float getBlend(float ShipEntry::*characteristic) const;
};

/// Where the ship equipped is kept in the settings, by its code ("VX-03"): the next run starts with it.
inline constexpr std::string_view EquippedShipSettingsKey = "ship.equipped";

/// Equips the ship kept in the settings, if it is still in the catalog and not locked.
void readEquippedShip(ShipSelection &selection, const engine::Settings &settings);
/// Keeps the ship equipped in the settings, and writes them. A file that cannot be written is logged: the
/// ship is then only remembered until the client closes.
void writeEquippedShip(const ShipSelection &selection, engine::Settings &settings);

/// How far the ship at `index` is from the carousel, in ships, around the catalog: negative on the left of
/// the carousel, positive on its right, from -3 to 3.
[[nodiscard]] float getShipOffset(std::size_t index, float slide);

/// How much the ship at `index` is the one in the middle: 1 on it, 0 one ship away or more.
[[nodiscard]] float getShipWeight(std::size_t index, float slide);

} // namespace rtype::client
