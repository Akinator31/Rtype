#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/render/ResourceManager.hpp"

#include <cstdint>
#include <vector>

namespace rtype::client
{

/// The look of the HUD: the pictures of a life left and of a life lost, and its font.
struct HudLook {
    render::TextureId life;
    render::TextureId lostLife;
    render::FontId font;
};

/// The line of another player in the HUD: its name, then its score.
struct HudAlly {
    ecs::entity_t name = 0;
    ecs::entity_t score = 0;
};

/// The HUD of a game scene, made of entities of its scene: the HudSystem places them and writes them each
/// frame, from the players of the session and its ping.
struct Hud {
    HudLook look;
    /// The shadows along the top and the bottom of the screen, under the HUD.
    ecs::entity_t topShadow = 0;
    ecs::entity_t bottomShadow = 0;
    /// One picture per life at the start, from the left.
    std::vector<ecs::entity_t> lives;
    /// "1P", above the score of the local player.
    ecs::entity_t pilot = 0;
    ecs::entity_t score = 0;
    /// Online, right of the score: the experience earned in the game, and the streak from 2 waves.
    ecs::entity_t xp = 0;
    /// One line per other player at most.
    std::vector<HudAlly> allies;
    /// The bars of the signal, from the shortest, left of the ping.
    std::vector<ecs::entity_t> signal;
    ecs::entity_t ping = 0;
};

/// Opens the HUD of the game in a scene (docs/architecture/client-game.md, "HUD"), from the Figma design: a
/// shadow along the top and the bottom of the screen; in the bottom left corner, the lives of the local
/// player, its slot and its score; in the bottom right corner, the score of each other player, and the ping
/// under them with its signal bars, online only. The events stay in the corner of the toasts, top right.
/// @param maxLives Lives at the start of the game: one picture each.
/// @return The entity of the Hud.
ecs::entity_t openHud(ecs::Ecs &ecs, engine::SceneId scene, const HudLook &look, std::uint8_t maxLives);

} // namespace rtype::client
