#pragma once

#include "ShipSelector/ShipCatalog.hpp"

#include <cstddef>

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

/// How far the ship at `index` is from the carousel, in ships, around the catalog: negative on the left of
/// the carousel, positive on its right, from -3 to 3.
[[nodiscard]] float getShipOffset(std::size_t index, float slide);

/// How much the ship at `index` is the one in the middle: 1 on it, 0 one ship away or more.
[[nodiscard]] float getShipWeight(std::size_t index, float slide);

} // namespace rtype::client
