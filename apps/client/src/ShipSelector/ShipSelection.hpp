#pragma once

#include "ShipSelector/ShipCatalog.hpp"
#include "rtype/engine/Settings/Settings.hpp"
#include "rtype/game/Loadout.hpp"
#include "rtype/game/ShipUnlock.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace rtype::client
{

/// What the player has chosen in the hangar: the ship looked at, and the one equipped, as indices in the
/// catalog, and its two weapons; and what it may equip: a ship or a weapon needs a level of the account
/// (config/hangar.lua).
struct ShipSelection {
    std::size_t focused = 0;
    std::size_t equipped = 0;
    /// The level of the account signed in, or of the last one signed in on this machine: offline and as a
    /// guest, the player keeps the ships it has unlocked. 1 until an account signs in.
    std::uint32_t level = 1;
    /// The level each ship of the catalog needs, in the order of the catalog: 0 and 1 are open to all.
    std::array<std::uint32_t, shipCount> requiredLevels{};
    /// Where the carousel is, as an index of the catalog that glides towards `focused`, around: 2.5 is
    /// halfway between the third and the fourth ship. Moved by the ShipSelectorSystem.
    float slide = 0.0f;
    /// What to do once the player has picked the ship of the game it starts: the hangar then launches the
    /// game instead of only equipping the ship. Empty when the hangar is opened to look at the ships.
    std::function<void()> launch;
    /// The ship picked for the game takes off (ShipCarousel): the ship and the weapons no longer change.
    bool departing = false;
    /// The take-off has flown the ship into the sky of the game, which has scrolled for this long, in
    /// seconds: the game opens on the shot the hangar shows (ArrivalZoom in Game/GameView.hpp), its sky going
    /// on, and takes it. None otherwise.
    std::optional<float> arrivalSky;
    /// The weapons of the hangar, in the order of config/hangar.lua, with the level each one needs.
    std::vector<game::WeaponUnlock> weapons;
    /// The two weapons equipped, as indices in `weapons`: different, and open at `level`, once the hangar
    /// has two weapons open.
    std::array<std::size_t, game::LoadoutSize> loadout{0, 1};

    /// Looks at the next ship (+1) or the previous one (-1), around; not while departing.
    void step(int direction);
    /// Equips the ship looked at, unless it is locked.
    void equipFocused();
    /// A locked ship can be looked at but not equipped: it needs a higher level than the player's.
    [[nodiscard]] bool isLocked(std::size_t index) const { return level < requiredLevels[index]; }
    /// Changes the level of the player; a ship equipped that it locks gives way to the first one open.
    void setLevel(std::uint32_t value);
    [[nodiscard]] bool isFocusedEquipped() const { return focused == equipped; }
    [[nodiscard]] const ShipEntry &getFocused() const { return getShipCatalog()[focused]; }

    /// The ship the carousel is nearest to: what the texts and the gauges show while it glides.
    [[nodiscard]] std::size_t getShownIndex() const;
    [[nodiscard]] const ShipEntry &getShown() const { return getShipCatalog()[getShownIndex()]; }
    /// 1 when the carousel rests on a ship, 0 halfway between two: the texts fade out and in while it glides.
    [[nodiscard]] float getShownClarity() const;
    /// A characteristic mixed between the two ships the carousel is between.
    [[nodiscard]] float getBlend(float ShipEntry::*characteristic) const;

    /// A locked weapon cannot be equipped: it needs a higher level than the player's.
    [[nodiscard]] bool isWeaponLocked(std::size_t index) const { return level < weapons[index].level; }
    /// Equips the next weapon (+1) or the previous one (-1) of the hangar at a place of the loadout (0 or 1),
    /// around, past the locked ones and the one at the other place. Nothing changes without another weapon
    /// to take, nor while departing.
    void stepWeapon(std::size_t place, int direction);
    /// The loadout to take into a game: the ids of the two weapons equipped. Empty, for the weapon of
    /// config/player.lua alone, while the hangar has fewer than two weapons open.
    [[nodiscard]] game::Loadout getLoadout() const;
};

/// Takes the levels the ships need from config/hangar.lua: a ship of the catalog it leaves out is open from
/// level 1, a code it names that is not in the catalog is logged.
void readRequiredLevels(ShipSelection &selection, const std::vector<game::ShipUnlock> &unlocks);

/// Where the level of the last account signed in is kept in the settings.
inline constexpr std::string_view PlayerLevelSettingsKey = "account.level";

/// Takes the weapons of the hangar from config/hangar.lua, the first two open equipped.
void readHangarWeapons(ShipSelection &selection, const std::vector<game::WeaponUnlock> &weapons);

/// Takes the level kept in the settings, if any.
void readPlayerLevel(ShipSelection &selection, const engine::Settings &settings);
/// Sets the level of the account signed in and keeps it in the settings, written only when it changes.
void writePlayerLevel(ShipSelection &selection, std::uint32_t level, engine::Settings &settings);

/// The ships of the catalog that are open at `level` but were locked at `before`, as indices of the catalog.
[[nodiscard]] std::vector<std::size_t> listUnlockedShips(const ShipSelection &selection, std::uint32_t before,
                                                         std::uint32_t level);

/// Where the ship equipped is kept in the settings, by its code ("VX-03"): the next run starts with it.
inline constexpr std::string_view EquippedShipSettingsKey = "ship.equipped";

/// Equips the ship kept in the settings, if it is still in the catalog and not locked. Read the levels first.
void readEquippedShip(ShipSelection &selection, const engine::Settings &settings);
/// Keeps the ship equipped in the settings, and writes them. A file that cannot be written is logged: the
/// ship is then only remembered until the client closes.
void writeEquippedShip(const ShipSelection &selection, engine::Settings &settings);

/// Where the weapons equipped are kept in the settings, by their ids: "player_cannon,rapid_rifle".
inline constexpr std::string_view EquippedWeaponsSettingsKey = "ship.weapons";

/// Equips the weapons kept in the settings, if they are still in the hangar, open and different. Read the
/// weapons and the level first.
void readEquippedWeapons(ShipSelection &selection, const engine::Settings &settings);
/// Keeps the weapons equipped in the settings, and writes them, as writeEquippedShip().
void writeEquippedWeapons(const ShipSelection &selection, engine::Settings &settings);

/// How far the ship at `index` is from the carousel, in ships, around the catalog: negative on the left of
/// the carousel, positive on its right, from -3 to 3.
[[nodiscard]] float getShipOffset(std::size_t index, float slide);

/// How much the ship at `index` is the one in the middle: 1 on it, 0 one ship away or more.
[[nodiscard]] float getShipWeight(std::size_t index, float slide);

} // namespace rtype::client
