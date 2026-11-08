#pragma once

#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/localization/Localization.hpp"

namespace rtype::client
{

/// Name of the Controls page, opened from the Settings page, and of the scene over it that asks for a key.
inline constexpr const char *ControlsMenuScene = "controls_menu";
inline constexpr const char *KeyCaptureScene = "key_capture";

/// Declares the Controls page: a table with one row per action of the game (GameActions), in two groups
/// (moving, firing), each with its keys and the buttons of the gamepad, then a button that puts the default
/// keys back. Pressing a row opens a scene over the page, which stays visible: the row waits for a key, the
/// others dim. A key no other action has is bound to the action alone (see assignKey()); a key another action
/// has turns red on that action, and Enter gives it to the chosen one, another key is tried instead, Escape
/// keeps the keys as they were. The keys are written to the settings of the engine when the page closes. Only
/// the keyboard is rebound: the gamepads keep their buttons, shown read only.
/// @param style, translations They must outlive the engine.
void createControlsScenes(engine::Engine &engine, const engine::InterfaceStyle &style,
                          localization::Localization &translations);

} // namespace rtype::client
