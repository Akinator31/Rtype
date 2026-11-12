#include "Vr/VrHaptics.hpp"

namespace rtype::client
{

std::vector<std::pair<xr::Hand, xr::Vibration>> VrHaptics::update(const ShipFeel &ship)
{
    std::vector<std::pair<xr::Hand, xr::Vibration>> vibrations;
    const std::optional<ShipFeel> previous =
        std::exchange(_previous, ship.playing ? std::optional(ship) : std::nullopt);
    // A game that starts counts from where it is: nothing happened yet
    if (!ship.playing || !previous || ship.shots < previous->shots) {
        return vibrations;
    }
    if (ship.shots > previous->shots) {
        vibrations.emplace_back(xr::Hand::Right, ShotTick);
    }
    if (previous->health > 0 && ship.health < previous->health) {
        const xr::Vibration &felt = ship.health == 0 ? DeathRumble : HitPulse;
        vibrations.emplace_back(xr::Hand::Left, felt);
        vibrations.emplace_back(xr::Hand::Right, felt);
    }
    return vibrations;
}

} // namespace rtype::client
