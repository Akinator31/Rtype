#pragma once

#include "rtype/xr/Headset.hpp"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace rtype::client
{

/// What the game says of the ship of the player each frame (GameSessionSystem), for the vibrations.
struct ShipFeel {
    /// A game is played.
    bool playing = false;
    /// How many shots of the player appeared since the game started (GameSessionSystem::localShots()).
    std::uint32_t shots = 0;
    /// Its hit points, 0 while it is not there (GameSessionSystem::localHealth()).
    std::uint8_t health = 0;
};

/// A short tick in the right hand, which fires (its trigger and A), at each shot: held, the shots of the game
/// repeat, and so do the ticks, the rhythm of the fire.
inline constexpr xr::Vibration ShotTick{.amplitude = 0.25f, .seconds = 0.015f, .frequency = 0.0f};
/// A pulse in both hands when the ship is hit.
inline constexpr xr::Vibration HitPulse{.amplitude = 0.7f, .seconds = 0.12f, .frequency = 0.0f};
/// A long one when it is destroyed.
inline constexpr xr::Vibration DeathRumble{.amplitude = 1.0f, .seconds = 0.6f, .frequency = 0.0f};

/// The vibrations of the controllers in a game (docs/architecture/vr.md#the-haptics): a tick on each shot of
/// the player, in the right hand, which fires; a pulse in both when their ship is hit, a long one when it is
/// destroyed. It reads what the game shows, nothing else: the game is not told.
class VrHaptics
{
    std::optional<ShipFeel> _previous;

public:
    /// Once per frame while the headset shows the game: the vibrations of what changed since the previous
    /// frame. A game that starts, or a ship that appears, does not vibrate.
    [[nodiscard]] std::vector<std::pair<xr::Hand, xr::Vibration>> update(const ShipFeel &ship);
};

} // namespace rtype::client
