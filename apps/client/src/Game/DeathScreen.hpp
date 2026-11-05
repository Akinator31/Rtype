#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/render/ResourceManager.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rtype::client
{

/// The look of the death screen: the pictures of a life left and of a life lost, its fonts, and its captions
/// in the language of the player.
struct DeathScreenLook {
    render::TextureId life;
    render::TextureId lostLife;
    render::FontId captionFont;
    render::FontId countdownFont;
    /// "LIVES LEFT", before the pictures of the lives.
    std::string livesCaption;
    /// "RESPAWN", above the countdown.
    std::string respawnCaption;
};

/// The ship of the local player lost, with lives left.
struct Death {
    /// Lives left, the one just lost not counted.
    std::uint8_t lives = 0;
    /// Lives at the start of the game: one picture each.
    std::uint8_t maxLives = 0;
    /// The tick the next ship of the player comes at.
    std::uint32_t respawnTick = 0;
    /// Ticks from the loss of the ship to the next one.
    std::uint32_t respawnDelay = 0;
};

/// The death screen showing, made of entities of its scene: the DeathScreenSystem fades them in, counts down
/// to the next ship, then fades them out and destroys them once it is back.
struct DeathScreen {
    Death death;
    ecs::entity_t veil = 0;
    /// Everything but the veil: it appears once the banner has opened its band.
    std::vector<ecs::entity_t> content;
    ecs::entity_t countdown = 0;
    ecs::entity_t ring = 0;
    /// The picture of a life left over the one of the life just lost: it goes out.
    ecs::entity_t lostLife = 0;
    /// Seconds since it opened.
    float time = 0.0f;
    /// Seconds since the next ship came: the screen fades out.
    std::optional<float> closing;
};

/// Opens the death screen of the local player in a scene (docs/architecture/client-game.md, "Death screen"):
/// a dark veil over the game, the row of its lives in the band of the banner, the life just lost going out,
/// and under the band the countdown to its next ship, a ring that runs out around the seconds left. The
/// banner itself, "SHIP DESTROYED", is a toast of the engine (Engine::showBanner()).
/// @param banner The style of the banners, to put the lives in their band.
/// @return The entity of the DeathScreen.
ecs::entity_t openDeathScreen(ecs::Ecs &ecs, engine::SceneId scene, const engine::Metrics &metrics,
                              const engine::ToastStyle &banner, const DeathScreenLook &look,
                              const Death &death);

} // namespace rtype::client
