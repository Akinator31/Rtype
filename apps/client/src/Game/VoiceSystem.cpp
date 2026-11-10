#include "Game/VoiceSystem.hpp"

namespace rtype::client
{

VoiceSystem::VoiceSystem(ecs::ComponentCaches &caches, const GameSessionSystem &sessions,
                         audio::AudioPlayer &audio)
    : _sessions(sessions), _audio(audio), _voices(use<ScheduledVoice>(caches))
{
}

void VoiceSystem::update(float /*elapsed*/)
{
    const std::uint32_t tick = _sessions.tick();
    for (ecs::entity_t entity = 0; entity < _voices.size(); ++entity) {
        if (_voices.has(entity) && _voices.get(entity).tick <= tick) {
            const ScheduledVoice &voice = _voices.get(entity);
            _audio.play(voice.sound, {.volume = voice.volume});
            _voices.erase(entity);
        }
    }
}

} // namespace rtype::client
