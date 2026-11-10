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
    // A lower level may have changed the ship equipped
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

void ShipSelection::step(int direction)
{
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
