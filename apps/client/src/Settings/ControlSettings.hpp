#pragma once

#include "rtype/engine/Control/Controls.hpp"
#include "rtype/engine/Settings/Settings.hpp"
#include "rtype/input/Keys.hpp"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace rtype::client
{

/// An action of the player that the Controls page lets rebind, with the control of the virtual gamepad the
/// game reads it from (see GameSessionSystem).
struct GameAction {
    engine::Control control;
    /// Its name in the translations ("controls.fire") and in the settings ("controls.fire=Space").
    std::string_view name;
};

/// The actions of the game, in the order of the Controls page.
inline constexpr std::array<GameAction, 7> GameActions{{
    {engine::Control::Up, "move_up"},
    {engine::Control::Down, "move_down"},
    {engine::Control::Left, "move_left"},
    {engine::Control::Right, "move_right"},
    {engine::Control::FaceDown, "fire"},
    {engine::Control::LeftShoulder, "aim_up"},
    {engine::Control::RightShoulder, "aim_down"},
}};

/// The start of the keys of the actions in the settings, and in the translations.
inline constexpr std::string_view ControlsSettingsPrefix = "controls.";

/// The key that cancels the choice of a key: it is never bound to an action of the game.
inline constexpr input::Key CancelKey = input::Key::Escape;

/// The keyboard keys of a control, in the order they were bound.
[[nodiscard]] std::vector<input::Key> keysOf(const engine::Controls &controls, engine::Control control);

/// Binds this key alone to the control, instead of its other keys; its gamepad inputs stay. The key leaves
/// the other GameActions that had it, so that a key does one thing in the game.
void assignKey(engine::Controls &controls, engine::Control control, input::Key key);

/// The GameAction other than `control` that has this key, which assignKey() would take from it: the Controls
/// page asks before. Nothing if no other action has it.
[[nodiscard]] const GameAction *findKeyOwner(const engine::Controls &controls, engine::Control control,
                                             input::Key key);

/// The keys in the translations of the gamepad inputs of a control, in the order they were bound, each once:
/// "pad.dpad_up" for the D-pad, "pad.left_stick" for both directions of the left stick, "pad.a" for the
/// bottom face button. The Controls page shows them; they are not rebound.
[[nodiscard]] std::vector<std::string> gamepadNamesOf(const engine::Controls &controls,
                                                      engine::Control control);

/// Gives the GameActions the keys kept in the settings. An action with nothing kept, or with a key that does
/// not exist, keeps its default keys.
void applyControlSettings(const engine::Settings &settings, engine::Controls &controls);
/// Keeps the keys of the GameActions in the settings, without writing them: "controls.move_up=Up,W".
void keepControlSettings(engine::Settings &settings, const engine::Controls &controls);

} // namespace rtype::client
