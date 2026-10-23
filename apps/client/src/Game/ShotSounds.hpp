#pragma once

#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/game/Snapshot.hpp"

#include <unordered_map>

namespace rtype::client
{

/// The sound of a kind of shot, played when one appears in the game: the weapon that fired it.
struct ShotSound {
    audio::SoundId sound;
    /// Between 0 and 1: the Bydos fire often, quieter than the players.
    float volume = 1.0f;
};

/// The ShotSound of each kind of shot that has one (docs/architecture/client-game.md, "Shot sounds"). The
/// engine plays them: a shot that appears is given an engine::SoundEffect, played by the engine::SoundSystem.
class ShotSounds
{
    std::unordered_map<game::EntityKind, ShotSound> _sounds;

public:
    void setSound(game::EntityKind kind, const ShotSound &sound);

    /// Gives the entity of a shot that appears the sound of its kind, heard from the side of the screen the
    /// shot is on. A kind without a sound is given nothing.
    void addSound(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state) const;
};

} // namespace rtype::client
