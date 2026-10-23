#include "rtype/engine/System/SoundSystem.hpp"

namespace rtype::engine
{

SoundSystem::SoundSystem(ecs::ComponentCaches &caches, audio::AudioPlayer &player)
    : _player(player), _effects(use<SoundEffect>(caches))
{
}

void SoundSystem::update(float /*elapsed*/)
{
    _player.update();
    for (ecs::entity_t entity = 0; entity < _effects.size(); ++entity) {
        if (_effects.has(entity)) {
            const SoundEffect &effect = _effects.get(entity);
            _player.play(effect.sound, effect.settings);
            _effects.erase(entity);
        }
    }
}

} // namespace rtype::engine
