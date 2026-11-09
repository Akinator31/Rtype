#pragma once

#include "Discord/DiscordPresence.hpp"
#include "Voice/VoiceDevices.hpp"
#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/render/Renderer.hpp"
#include "rtype/render/Window.hpp"
#include "rtype/voice/Microphone.hpp"

namespace rtype::client
{

/// What the settings of the player change: the language, the volumes, the image, the window, the voice, and
/// the activity shown in Discord. Everything must outlive the engine.
struct SettingsTargets {
    localization::Localization &translations;
    audio::AudioPlayer &audio;
    render::Renderer &renderer;
    render::Window &window;
    /// How the player talks, read by the games online.
    VoiceDevices &voice;
    /// The microphone of VoiceDevices: the Voice page lists the others, and tries it.
    voice::Microphone &microphone;
    PresenceTracker &presence;
};

} // namespace rtype::client
