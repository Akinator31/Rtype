#pragma once

#include "Game/GameSessionSystem.hpp"
#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"

#include <cstdint>

namespace rtype::client
{

/// A voice to play once the game session reaches a tick: the voices of the rescue, one after the other.
struct ScheduledVoice {
    audio::SoundId sound;
    /// The tick of the session at which it plays.
    std::uint32_t tick = 0;
    float volume = 1.0f;
};

/// Plays the ScheduledVoice of each entity once the session played by the GameSessionSystem reaches its tick,
/// then removes it from the entity, which stays. A paused game holds them back.
class VoiceSystem : public ecs::System
{
    const GameSessionSystem &_sessions;
    audio::AudioPlayer &_audio;
    ecs::ComponentCache<ScheduledVoice> &_voices;

public:
    VoiceSystem(ecs::ComponentCaches &caches, const GameSessionSystem &sessions, audio::AudioPlayer &audio);

    void update(float elapsed) override;
};

} // namespace rtype::client
