#include "Game/EntitySounds.hpp"

#include "rtype/engine/Component/Sound.hpp"
#include "rtype/game/Constants.hpp"

namespace rtype::client
{

namespace
{

/// How far to the sides the entities are heard, at the edges of the game area: fully on one side would be
/// hard to hear with one ear.
constexpr float PanSpread = 0.6f;

} // namespace

void EntitySounds::setSound(game::EntityKind kind, const EntitySound &sound)
{
    _sounds.insert_or_assign(kind, sound);
}

void EntitySounds::addSound(ecs::Ecs &ecs, ecs::entity_t entity, game::EntityKind kind, float x) const
{
    const auto found = _sounds.find(kind);
    if (found == _sounds.end()) {
        return;
    }
    const float pan = ((x / game::GameWidth) * 2.0f - 1.0f) * PanSpread;
    ecs.addComponent<engine::SoundEffect>(
        entity, found->second.sound, audio::PlaySettings{.volume = found->second.volume, .pan = pan});
}

} // namespace rtype::client
