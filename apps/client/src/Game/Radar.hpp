#pragma once

#include "rtype/game/Snapshot.hpp"
#include "rtype/render/Types.hpp"

#include <cstdint>
#include <optional>

/// The radar of the board on the hologram of the cockpit, in a VR headset (Figma, Série 17): the game area
/// seen from above, small, so that the player sees where the others are without looking up at the board.
namespace rtype::client
{

/// The area of the hologram (engine::Surface::Holo), in game units of its surface.
inline constexpr render::Vector2 RadarSurface{480.0f, 330.0f};
/// The frame of the board on the radar, in the area of the hologram: of the shape of the game.
inline constexpr render::Rectangle RadarFrame{20.0f, 56.0f, 440.0f, 248.0f};
/// How many entities of the game the radar shows at most: one entity of the HUD each, made with it.
inline constexpr std::size_t RadarBlips = 48;

/// An entity of the game on the radar.
struct RadarBlip {
    /// Its centre, in the area of the hologram.
    render::Vector2 centre;
    /// The side of its square, in game units of the hologram.
    float size = 0.0f;
    render::Color color;
    /// Drawn as a frame instead of a filled square: the boss.
    bool outlined = false;
};

/// How an entity of the game shows on the radar: the ship of the player in white, larger, the other players
/// and the allies in cyan, the Bydos in red, the boss in orange, larger still and framed. The shots are not
/// shown: none.
/// @param owner The slot of the player the entity belongs to, or game::NoOwner.
/// @param local The slot of the local player.
/// @param position Where the entity is in the game, in game units.
/// @param gameSize Size of the game, which the frame of the radar shows whole.
[[nodiscard]] std::optional<RadarBlip> radarBlip(game::EntityKind kind, std::uint8_t owner,
                                                 std::uint8_t local, render::Vector2 position,
                                                 render::Vector2 gameSize);

} // namespace rtype::client
