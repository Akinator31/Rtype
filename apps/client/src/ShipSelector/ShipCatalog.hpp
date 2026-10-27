#pragma once

#include <array>
#include <cstddef>

namespace rtype::client
{

/// A ship the player can pick in the hangar.
struct ShipEntry {
    /// Short name written before its name, such as "VX-03".
    const char *code;
    /// Key of its name in the translations.
    const char *nameKey;
    /// Path of its model, relative to the assets directory.
    const char *modelPath;
    /// Size factor of the model on its pedestal: the models are 25 to 40 units long.
    float modelScale;
    /// Characteristics, from 0 to 1, shown as gauges.
    float speed;
    float power;
    float armor;
    /// A locked ship can be looked at but not equipped.
    bool locked;
};

inline constexpr std::size_t shipCount = 6;

/// The ships of the hangar, in the order of the carousel.
[[nodiscard]] const std::array<ShipEntry, shipCount> &getShipCatalog();

} // namespace rtype::client
