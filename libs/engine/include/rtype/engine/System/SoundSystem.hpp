#pragma once

#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Sound.hpp"

namespace rtype::engine
{

/// The only system that plays sounds, as the SceneRenderer is the only one that draws: the others give a
/// SoundEffect to an entity. Each update, it plays every SoundEffect and removes it, and feeds the stream of
/// the music of the player (audio::AudioPlayer::update()).
/// @code
/// ecs.registerSystem<SoundSystem>(audio); // after the systems that give sounds, to play them in the frame
/// const audio::SoundId fire = audio.loadSound("sounds/shot.wav");
/// ecs.addComponent<SoundEffect>(shot, fire, audio::PlaySettings{.volume = 0.5f});
/// @endcode
class SoundSystem : public ecs::System
{
    audio::AudioPlayer &_player;
    ecs::ComponentCache<SoundEffect> &_effects;

public:
    /// @param player It must outlive the system.
    SoundSystem(ecs::ComponentCaches &caches, audio::AudioPlayer &player);

    void update(float elapsed) override;
};

} // namespace rtype::engine
