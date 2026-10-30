#include "ShipSelector/ShipSelection.hpp"

#include "rtype/log/Logger.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
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

} // namespace

void readEquippedShip(ShipSelection &selection, const engine::Settings &settings)
{
    const std::optional<std::string> code = settings.getString(EquippedShipSettingsKey);
    if (!code) {
        return;
    }
    const auto &catalog = getShipCatalog();
    for (std::size_t index = 0; index < shipCount; ++index) {
        if (*code == catalog[index].code && !catalog[index].locked) {
            selection.equipped = index;
            selection.focused = index;
            return;
        }
    }
}

void writeEquippedShip(const ShipSelection &selection, engine::Settings &settings)
{
    settings.setString(EquippedShipSettingsKey, getShipCatalog()[selection.equipped].code);
    try {
        settings.writeFile();
    } catch (const std::exception &error) {
        logger().error("Cannot write the settings {}: {}", settings.getPath().string(), error.what());
    }
}

void ShipSelection::step(int direction)
{
    const std::size_t offset = direction >= 0 ? 1 : shipCount - 1;
    focused = (focused + offset) % shipCount;
}

void ShipSelection::equipFocused()
{
    if (!getFocused().locked) {
        equipped = focused;
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
