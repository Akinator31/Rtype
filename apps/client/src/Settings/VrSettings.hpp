#pragma once

#include "Vr/VrLayout.hpp"
#include "rtype/engine/Settings/Settings.hpp"

#include <array>
#include <functional>
#include <optional>
#include <string_view>

namespace rtype::client
{

/// Where the choices of the VR page are kept in the settings of the engine
/// (docs/architecture/vr.md#the-vr-page).
inline constexpr std::string_view VrSettingsKey = "vr.enabled";
inline constexpr std::string_view VrFlatBoardKey = "vr.flat_board";
inline constexpr std::string_view VrBoardDistanceKey = "vr.board_distance";
inline constexpr std::string_view VrBoardWidthKey = "vr.board_width";
inline constexpr std::string_view VrSeatHeightKey = "vr.seat_height";
inline constexpr std::string_view VrMirrorKey = "vr.mirror";
inline constexpr std::string_view VrSnapTurnKey = "vr.snap_turn";

/// What the window shows while the headset shows the game.
enum class VrMirror {
    /// What the left eye sees, cropped to the window: the people around see what the player sees.
    LeftEye,
    /// The game as on the board, flat, without the cockpit: drawn once more, for the window.
    FlatGame,
    /// Nothing but a line that says the game is in the headset: the cheapest, for a weaker computer.
    Nothing,
};

/// The mirrors, in the order the VR page goes through them.
inline constexpr std::array<VrMirror, 3> VrMirrors{VrMirror::LeftEye, VrMirror::FlatGame, VrMirror::Nothing};

/// The name of a mirror in the settings and in the keys of the translations: "left_eye", "flat_game",
/// "nothing".
[[nodiscard]] std::string_view mirrorName(VrMirror mirror);
/// The mirror of a name, none for a name that is not one.
[[nodiscard]] std::optional<VrMirror> mirrorFromName(std::string_view name);

/// The distances of the board offered, in metres, from the eyes to its centre. The nearest stays outside the
/// canopy, whose nose is 1.5 m ahead, with the frame of the board in relief (85 % of its distance) too.
inline constexpr std::array<float, 5> BoardDistances{1.8f, 2.0f, 2.3f, 2.6f, 3.0f};
/// The widths of the board offered, in metres.
inline constexpr std::array<float, 5> BoardWidths{2.2f, 2.45f, 2.7f, 3.0f, 3.3f};
/// How much higher than the eye point of the cockpit the player sits, in metres: a negative height sits
/// them lower. The cockpit moves the other way around their head.
inline constexpr std::array<float, 5> SeatHeights{-0.1f, -0.05f, 0.0f, 0.05f, 0.1f};
/// The steps of the snap turn offered, in degrees; 0 turns it off.
inline constexpr std::array<int, 4> SnapTurns{0, 30, 45, 90};

/// The choices of the VR page. Its defaults are those of a first run.
struct VrPreferences {
    /// The client starts in the headset (and the page opens it, or closes it, at once).
    bool enabled = false;
    /// The world is drawn once into the image of a flat board, instead of in relief for each eye.
    bool flatBoard = false;
    float boardDistance = VrLayout{}.boardDistance;
    float boardWidth = VrLayout{}.boardWidth;
    float seatHeight = 0.0f;
    VrMirror mirror = VrMirror::LeftEye;
    /// How far a flick of the right thumbstick turns the player, in degrees; 0 for no snap turn.
    int snapTurn = 0;

    bool operator==(const VrPreferences &) const = default;
};

/// The choices of the VR page kept in the settings. A value that is missing or wrong leaves its default; a
/// distance, a width, a height or a step that is not offered becomes the nearest one offered.
[[nodiscard]] VrPreferences readVrSettings(const engine::Settings &settings);
/// Keeps the choices of the VR page in the settings, without writing them.
void keepVrSettings(engine::Settings &settings, const VrPreferences &preferences);

/// Where the board and the panel float, and how high the player sits, for these choices.
[[nodiscard]] VrLayout layoutOf(const VrPreferences &preferences);

/// What the VR page changes. Everything must outlive the engine.
struct VrTargets {
    /// The choices of the player, read by the client every frame.
    VrPreferences &preferences;
    /// Opens the headset (true) or closes it (false), at once. True when it is open after the call: false
    /// when it was asked to open and there is none.
    std::function<bool(bool open)> switchHeadset;
    /// Puts the cockpit back around the player, facing where they look.
    std::function<void()> recenter;
};

} // namespace rtype::client
