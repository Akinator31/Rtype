#pragma once

#include "rtype/audio/AudioPlayer.hpp"

namespace rtype::engine
{

/// A sound effect to play once, from the entity it is given to: a shot that fires, a button that clicks. The
/// SoundSystem plays it at its next update, then removes it from the entity, which stays.
struct SoundEffect {
    audio::SoundId sound;
    audio::PlaySettings settings;
};

} // namespace rtype::engine
