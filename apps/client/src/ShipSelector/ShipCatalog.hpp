#pragma once

#include "rtype/render/Types.hpp"

#include <array>
#include <cstddef>
#include <vector>

namespace rtype::client
{

/// A ship the player can pick in the hangar, and fly in the game once equipped. The level it needs is in
/// config/hangar.lua (ShipSelection::requiredLevels).
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
    /// The box around the hull of the model seen through the camera of the game, flames excluded, in model
    /// units, measured on the model in its Move pose facing right (ShipLook::hull).
    render::Rectangle hull;
    /// The meshes of the trails behind its flames, by index in the model file (ShipLook::trailMeshes).
    std::vector<int> trailMeshes;
};

inline constexpr std::size_t shipCount = 6;

/// The ships of the hangar, in the order of the carousel.
[[nodiscard]] const std::array<ShipEntry, shipCount> &getShipCatalog();

} // namespace rtype::client
