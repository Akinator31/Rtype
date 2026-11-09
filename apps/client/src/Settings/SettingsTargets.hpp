#pragma once

#include "Discord/DiscordPresence.hpp"
#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/render/Renderer.hpp"
#include "rtype/render/Window.hpp"

namespace rtype::client
{

/// What the settings of the player change: the language, the volumes, the image, the window, and the activity
/// shown in Discord. Everything must outlive the engine.
struct SettingsTargets {
    localization::Localization &translations;
    audio::AudioPlayer &audio;
    render::Renderer &renderer;
    render::Window &window;
    PresenceTracker &presence;
};

} // namespace rtype::client
