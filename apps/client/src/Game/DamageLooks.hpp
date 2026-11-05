#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/game/Snapshot.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <cstdint>
#include <unordered_map>

namespace rtype::client
{

/// How the damage taken by the entities of a kind is written where they are hit: a number that pops out,
/// rises and fades out.
struct DamageLook {
    render::FontId font;
    /// Height of the text, in game units.
    float size = 40.0f;
    render::Color color = render::colors::White;
};

/// A number of damage showing: the DamageNumberSystem moves it up, fades it out at its end, then destroys it.
struct DamageNumber {
    /// How long it shows, in seconds.
    float duration = 0.0f;
    /// Seconds since it appeared.
    float time = 0.0f;
};

/// The DamageLook of each kind of entity whose damage is shown, and the numbers they write in the game scene
/// (docs/architecture/client-game.md, "Damage numbers").
class DamageLooks
{
    std::unordered_map<game::EntityKind, DamageLook> _looks;

public:
    /// How long a number shows, in seconds.
    static constexpr float Duration = 0.8f;

    void setLook(game::EntityKind kind, const DamageLook &look);
    /// The damage of the entities of this kind is shown.
    [[nodiscard]] bool hasLook(game::EntityKind kind) const;

    /// Creates the number of the damage taken by an entity of this kind hit at a position of the game, in a
    /// scene, above the ships and their impacts.
    ecs::entity_t createNumber(ecs::Ecs &ecs, engine::SceneId scene, game::EntityKind kind,
                               ecs::Position position, std::uint8_t damage) const;
};

} // namespace rtype::client
