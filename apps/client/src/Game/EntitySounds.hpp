#pragma once

#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/game/Snapshot.hpp"

#include <unordered_map>

namespace rtype::client
{

/// The sound of a kind of entity of the game, for one moment of its life: a shot fired, a ship destroyed.
struct EntitySound {
    audio::SoundId sound;
    /// Between 0 and 1: the Bydos fire often, quieter than the players.
    float volume = 1.0f;
};

/// The EntitySound of each kind of entity that has one, for one moment of their life (docs/architecture/
/// client-game.md, "Sounds"). The engine plays them: the entity that sounds is given an engine::SoundEffect,
/// played by the engine::SoundSystem.
class EntitySounds
{
    std::unordered_map<game::EntityKind, EntitySound> _sounds;

public:
    void setSound(game::EntityKind kind, const EntitySound &sound);

    /// Gives an entity the sound of a kind, heard from the side of the screen it is on, at a position of the
    /// game. A kind without a sound gives nothing.
    void addSound(ecs::Ecs &ecs, ecs::entity_t entity, game::EntityKind kind, float x) const;
};

} // namespace rtype::client
