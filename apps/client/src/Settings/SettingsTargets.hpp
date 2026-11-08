#pragma once

#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/render/Renderer.hpp"
#include "rtype/render/Window.hpp"

namespace rtype::client
{

/// What the settings of the player change: the language, the volumes, the image and the window. Everything
/// must outlive the engine.
struct SettingsTargets {
    localization::Localization &translations;
    audio::AudioPlayer &audio;
    render::Renderer &renderer;
    render::Window &window;
};

} // namespace rtype::client
