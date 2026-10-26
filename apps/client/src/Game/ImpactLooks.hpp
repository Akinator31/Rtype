#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/game/Snapshot.hpp"
#include "rtype/render/Energy.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <cstdint>
#include <unordered_map>

namespace rtype::client
{

/// What is left where an entity of a kind was destroyed in the game (game::DestroyCause::Killed): the impact
/// of a shot that hit, the explosion of a ship. A skinned model of energy that plays an animation once.
struct ImpactLook {
    render::ModelId model;
    /// Its animation (render::ResourceManager::getModelAnimation()).
    int animation = 0;
    /// Length of the animation, in seconds (render::ResourceManager::getModelAnimationDuration()).
    float duration = 0.0f;
    /// Game units per model unit.
    float scale = 1.0f;
    /// Colour of the energy: the impact models of the shots are white, drawn in the colour of their shot.
    render::Color tint = render::colors::White;
    render::Energy energy;
};

/// An impact playing: the ImpactSystem moves its animation, fades it out at its end, then destroys it.
struct Impact {
    /// Length of the animation, in seconds.
    float duration = 0.0f;
    /// Seconds since it started.
    float time = 0.0f;
    /// Opacity of its model before it fades out.
    std::uint8_t alpha = 255;
};

/// The ImpactLook of each kind of entity that leaves one, and the impacts they create in the game scene
/// (docs/architecture/client-game.md, "Impacts").
class ImpactLooks
{
    std::unordered_map<game::EntityKind, ImpactLook> _looks;

public:
    void setLook(game::EntityKind kind, const ImpactLook &look);
    /// The entities of this kind leave an impact.
    [[nodiscard]] bool hasLook(game::EntityKind kind) const;

    /// Creates the impact of an entity of this kind destroyed at a position of the game, in a scene, in front
    /// of the ships.
    ecs::entity_t createImpact(ecs::Ecs &ecs, engine::SceneId scene, game::EntityKind kind,
                               ecs::Position position) const;
};

} // namespace rtype::client
