#pragma once

#include "Settings/SettingsTargets.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Style.hpp"

namespace rtype::client
{

/// Name of the Voice page, opened from the Settings page, and of the scene over it that asks for a key.
inline constexpr const char *VoiceMenuScene = "voice_menu";
inline constexpr const char *VoiceKeyCaptureScene = "voice_key_capture";

/// Declares the Voice page (docs/architecture/voice.md#the-voice-page): on the left, how the player talks
/// (the mode, the threshold of the voice activity over the level of the microphone, which is open while the
/// page is, to try it) and the microphone (which one, its volume); on the right, the volume of the voices
/// and the keys of the voice (push-to-talk, mute, deafen), chosen as on the Controls page. Each choice
/// applies at once; they are written to the settings of the engine when the page closes (see
/// VoiceSettings.hpp and ControlSettings.hpp).
/// @param style Look of the interface. It must outlive the engine.
void createVoiceScenes(engine::Engine &engine, const engine::InterfaceStyle &style, SettingsTargets targets);

} // namespace rtype::client
