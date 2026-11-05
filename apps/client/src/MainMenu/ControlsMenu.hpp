#pragma once

#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/localization/Localization.hpp"

namespace rtype::client
{

/// Name of the Controls page, opened from the Settings page, and of the scene over it that asks for a key.
inline constexpr const char *ControlsMenuScene = "controls_menu";
inline constexpr const char *KeyCaptureScene = "key_capture";

/// Declares the Controls page: one row per action of the game (GameActions) with its keys, then a row that
/// puts the default keys back. Pressing a row opens a scene that waits for a key, and binds it to the action
/// alone (see assignKey()); Escape cancels. The keys are written to the settings of the engine when the page
/// closes. Only the keyboard is rebound: the gamepads keep their buttons.
/// @param style, translations They must outlive the engine.
void createControlsScenes(engine::Engine &engine, const engine::InterfaceStyle &style,
                          localization::Localization &translations);

} // namespace rtype::client
