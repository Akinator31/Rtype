#include "ShipSelector/ShipSelection.hpp"

#include "rtype/log/Logger.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <optional>
#include <string>

namespace rtype::client
{

namespace
{

constexpr auto count = static_cast<float>(shipCount);

/// The value taken back between -count / 2 and count / 2, a whole number of turns of the catalog away.
float wrapAround(float value)
{
    return value - (count * std::round(value / count));
}

const log::Logger &logger()
{
    static const log::Logger instance("ships");
    return instance;
}

/// The index of the ship of the catalog with this code, shipCount when there is none.
std::size_t findShip(std::string_view code)
{
    const auto &catalog = getShipCatalog();
    for (std::size_t index = 0; index < shipCount; ++index) {
        if (code == catalog[index].code) {
            return index;
        }
    }
    return shipCount;
}

void writeSettings(engine::Settings &settings)
{
    try {
        settings.writeFile();
    } catch (const std::exception &error) {
        logger().error("Cannot write the settings {}: {}", settings.getPath().string(), error.what());
    }
}

/// The index of the weapon of the hangar with this id, the number of weapons when there is none.
std::size_t findWeapon(const ShipSelection &selection, std::string_view id)
{
    const auto found = std::ranges::find(selection.weapons, id, &game::WeaponUnlock::weapon);
    return static_cast<std::size_t>(found - selection.weapons.begin());
}

/// Whether the weapon at `index` can be at a place of the loadout: open, and not at the other place.
bool canEquipWeapon(const ShipSelection &selection, std::size_t place, std::size_t index)
{
    return index < selection.weapons.size() && !selection.isWeaponLocked(index) &&
           selection.loadout[1 - place] != index;
}

/// Puts an open weapon at each place of the loadout that has a locked one, or the same as the first place.
void fixLoadout(ShipSelection &selection)
{
    for (std::size_t place = 0; place < game::LoadoutSize; ++place) {
        const std::size_t current = selection.loadout[place];
        if (current < selection.weapons.size() && !selection.isWeaponLocked(current) &&
            (place == 0 || current != selection.loadout[0])) {
            continue;
        }
        for (std::size_t index = 0; index < selection.weapons.size(); ++index) {
            if (canEquipWeapon(selection, place, index)) {
                selection.loadout[place] = index;
                break;
            }
        }
    }
}

/// Keeps the weapons equipped in the settings, without writing them.
void keepEquippedWeapons(const ShipSelection &selection, engine::Settings &settings)
{
    const game::Loadout loadout = selection.getLoadout();
    if (!loadout[0].empty()) {
        settings.setString(EquippedWeaponsSettingsKey, loadout[0] + "," + loadout[1]);
    }
}

} // namespace

void readRequiredLevels(ShipSelection &selection, const std::vector<game::ShipUnlock> &unlocks)
{
    for (const game::ShipUnlock &unlock : unlocks) {
        const std::size_t index = findShip(unlock.ship);
        if (index == shipCount) {
            logger().warning("hangar.lua: no ship '{}' in the hangar", unlock.ship);
            continue;
        }
        selection.requiredLevels[index] = unlock.level;
    }
    selection.setLevel(selection.level);
}

void readHangarWeapons(ShipSelection &selection, const std::vector<game::WeaponUnlock> &weapons)
{
    selection.weapons = weapons;
    selection.loadout = {0, 1};
    selection.setLevel(selection.level);
}

void readPlayerLevel(ShipSelection &selection, const engine::Settings &settings)
{
    const std::optional<int> level = settings.getInt(PlayerLevelSettingsKey);
    if (level && *level >= 1) {
        selection.setLevel(static_cast<std::uint32_t>(*level));
    }
}

void writePlayerLevel(ShipSelection &selection, std::uint32_t level, engine::Settings &settings)
{
    selection.setLevel(level);
    const int kept = static_cast<int>(std::min<std::uint32_t>(level, std::numeric_limits<int>::max()));
    if (settings.getInt(PlayerLevelSettingsKey) == kept) {
        return;
    }
    settings.setInt(PlayerLevelSettingsKey, kept);
    // A lower level may have changed the ship and the weapons equipped
    keepEquippedWeapons(selection, settings);
    writeEquippedShip(selection, settings);
}

std::vector<std::size_t> listUnlockedShips(const ShipSelection &selection, std::uint32_t before,
                                           std::uint32_t level)
{
    std::vector<std::size_t> unlocked;
    for (std::size_t index = 0; index < shipCount; ++index) {
        const std::uint32_t required = selection.requiredLevels[index];
        if (required > before && required <= level) {
            unlocked.push_back(index);
        }
    }
    return unlocked;
}

void readEquippedShip(ShipSelection &selection, const engine::Settings &settings)
{
    const std::optional<std::string> code = settings.getString(EquippedShipSettingsKey);
    if (!code) {
        return;
    }
    const std::size_t index = findShip(*code);
    if (index != shipCount && !selection.isLocked(index)) {
        selection.equipped = index;
        selection.focused = index;
    }
}

void writeEquippedShip(const ShipSelection &selection, engine::Settings &settings)
{
    settings.setString(EquippedShipSettingsKey, getShipCatalog()[selection.equipped].code);
    writeSettings(settings);
}

void readEquippedWeapons(ShipSelection &selection, const engine::Settings &settings)
{
    const std::optional<std::string> kept = settings.getString(EquippedWeaponsSettingsKey);
    const std::size_t comma = kept ? kept->find(',') : std::string::npos;
    if (comma == std::string::npos) {
        return;
    }
    const std::size_t first = findWeapon(selection, std::string_view(*kept).substr(0, comma));
    const std::size_t second = findWeapon(selection, std::string_view(*kept).substr(comma + 1));
    const std::size_t count = selection.weapons.size();
    if (first < count && second < count && first != second && !selection.isWeaponLocked(first) &&
        !selection.isWeaponLocked(second)) {
        selection.loadout = {first, second};
    }
}

void writeEquippedWeapons(const ShipSelection &selection, engine::Settings &settings)
{
    keepEquippedWeapons(selection, settings);
    writeSettings(settings);
}

void ShipSelection::step(int direction)
{
    if (departing) {
        return;
    }
    const std::size_t offset = direction >= 0 ? 1 : shipCount - 1;
    focused = (focused + offset) % shipCount;
}

void ShipSelection::equipFocused()
{
    if (!isLocked(focused)) {
        equipped = focused;
    }
}

void ShipSelection::setLevel(std::uint32_t value)
{
    level = value;
    for (std::size_t index = 0; isLocked(equipped) && index < shipCount; ++index) {
        if (!isLocked(index)) {
            equipped = index;
        }
    }
    fixLoadout(*this);
}

void ShipSelection::stepWeapon(std::size_t place, int direction)
{
    const std::size_t count = weapons.size();
    if (departing || place >= game::LoadoutSize || count == 0) {
        return;
    }
    const std::size_t offset = direction >= 0 ? 1 : count - 1;
    std::size_t index = loadout[place];
    for (std::size_t tried = 1; tried < count; ++tried) {
        index = (index + offset) % count;
        if (canEquipWeapon(*this, place, index)) {
            loadout[place] = index;
            return;
        }
    }
}

game::Loadout ShipSelection::getLoadout() const
{
    const auto open = [this](std::size_t index) { return index < weapons.size() && !isWeaponLocked(index); };
    if (!open(loadout[0]) || !open(loadout[1]) || loadout[0] == loadout[1]) {
        return {};
    }
    return {weapons[loadout[0]].weapon, weapons[loadout[1]].weapon};
}

std::size_t ShipSelection::getShownIndex() const
{
    return static_cast<std::size_t>(std::lround(wrapAround(slide) + count)) % shipCount;
}

float ShipSelection::getShownClarity() const
{
    const float distance = std::abs(slide - std::round(slide));
    return std::clamp(1.0f - (2.8f * distance), 0.0f, 1.0f);
}

float ShipSelection::getBlend(float ShipEntry::*characteristic) const
{
    const float wrapped = slide - (count * std::floor(slide / count));
    const auto first = static_cast<std::size_t>(wrapped) % shipCount;
    const float amount = wrapped - std::floor(wrapped);
    const auto &catalog = getShipCatalog();
    const float from = catalog[first].*characteristic;
    const float to = catalog[(first + 1) % shipCount].*characteristic;
    return from + ((to - from) * amount);
}

float getShipOffset(std::size_t index, float slide)
{
    return wrapAround(static_cast<float>(index) - slide);
}

float getShipWeight(std::size_t index, float slide)
{
    return std::clamp(1.0f - std::abs(getShipOffset(index, slide)), 0.0f, 1.0f);
}

} // namespace rtype::client
