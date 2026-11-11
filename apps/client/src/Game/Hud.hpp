#pragma once

#include "Voice/VoiceIcons.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace rtype::client
{

/// The words of the HUD on the screens of the cockpit, in the language of the player.
struct HudWords {
    /// Over the number of the wave, and after it in an endless game.
    std::string wave = "WAVE";
    std::string endless = "ENDLESS";
    /// While a boss is in the game.
    std::string boss = "BOSS APPROACHING";
    /// The kind of game, and the title of the other pilots, or what is shown without any.
    std::string online = "ONLINE";
    std::string solo = "SOLO";
    std::string pilots = "PILOTS";
    std::string alone = "FLYING SOLO";
};

/// The look of the HUD: the pictures of a life left and of a life lost, its font, the icons of the voice, and
/// its words on the screens of the cockpit.
struct HudLook {
    render::TextureId life;
    render::TextureId lostLife;
    render::FontId font;
    VoiceIconTextures voice{};
    HudWords words{};
};

/// The size of a text of the HUD and the space between its letters, in game units.
struct HudText {
    float size = 0.0f;
    float spacing = 0.0f;
};

/// The sizes of the parts of the HUD: on the screen, from the Figma design 1440 wide scaled to the 1920 of
/// the game, and on the screens of the cockpit (Série 17), larger, to be read at arm's length.
struct HudScale {
    HudText pilot;
    HudText score;
    HudText health;
    HudText xp;
    /// The slot of another player ("P2"), then its name and its score.
    HudText slot;
    HudText ally;
    HudText ping;
    render::Vector2 life;
    float pilotVoice = 0.0f;
    float allyVoice = 0.0f;
    float barWidth = 0.0f;
    std::array<float, 4> bars{};
};

inline constexpr HudScale ScreenHud{.pilot = {13.3f, 2.1f},
                                    .score = {34.7f, 4.0f},
                                    .health = {20.0f, 2.0f},
                                    .xp = {12.0f, 1.3f},
                                    .slot = {12.0f, 0.0f},
                                    .ally = {12.0f, 1.3f},
                                    .ping = {13.3f, 1.3f},
                                    .life = {40.0f, 16.0f},
                                    .pilotVoice = 16.0f,
                                    .allyVoice = 14.7f,
                                    .barWidth = 4.0f,
                                    .bars = {6.7f, 10.7f, 14.7f, 18.7f}};

inline constexpr HudScale CockpitHud{.pilot = {22.0f, 3.0f},
                                     .score = {50.0f, 5.0f},
                                     .health = {40.0f, 3.0f},
                                     .xp = {22.0f, 2.0f},
                                     .slot = {20.0f, 1.0f},
                                     .ally = {22.0f, 2.0f},
                                     .ping = {24.0f, 2.0f},
                                     .life = {60.0f, 24.0f},
                                     .pilotVoice = 26.0f,
                                     .allyVoice = 26.0f,
                                     .barWidth = 8.0f,
                                     .bars = {12.0f, 20.0f, 28.0f, 36.0f}};

/// The line of another player in the HUD, from the right: its score, its name online, its slot ("P2"), and
/// the icon of its voice.
struct HudAlly {
    ecs::entity_t slot = 0;
    ecs::entity_t name = 0;
    ecs::entity_t score = 0;
    ecs::entity_t voice = 0;
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
    /// "1P", with its name online, above the score of the local player, and the icon of its voice after it.
    ecs::entity_t pilot = 0;
    ecs::entity_t pilotVoice = 0;
    ecs::entity_t score = 0;
    /// The hit points of the ship of the local player, in green, under the score.
    ecs::entity_t health = 0;
    /// Online, right of the score: the experience earned in the game, and the streak from 2 waves.
    ecs::entity_t xp = 0;
    /// One line per other player at most.
    std::vector<HudAlly> allies;
    /// The bars of the signal, from the shortest, left of the ping.
    std::vector<ecs::entity_t> signal;
    ecs::entity_t ping = 0;

    // Only on the screens of the cockpit, in a VR headset (Figma, Série 17): hidden on the screen
    /// The navigation screen: "WAVE" over the number of the wave, the alert of a boss, and the kind of game
    /// after its light.
    ecs::entity_t waveLabel = 0;
    ecs::entity_t wave = 0;
    ecs::entity_t boss = 0;
    ecs::entity_t modeLight = 0;
    ecs::entity_t mode = 0;
    /// The title of the screens of the sides, over the other pilots, or alone in their middle.
    ecs::entity_t pilots = 0;
    /// The radar on the hologram (Game/Radar.hpp): its title, the frame of the board, its grid, and a
    /// square per entity of the game shown, RadarBlips at most.
    ecs::entity_t radarTitle = 0;
    ecs::entity_t radarFrame = 0;
    std::vector<ecs::entity_t> radarGrid;
    std::vector<ecs::entity_t> radarBlips;
};

/// Opens the HUD of the game in a scene (docs/architecture/client-game.md, "HUD"), from the Figma design: a
/// shadow along the top and the bottom of the screen; in the bottom left corner, the lives of the local
/// player, its slot and its score; in the bottom right corner, the score of each other player, and the ping
/// under them with its signal bars, online only. Online with a voice, the icon of the voice of each pilot is
/// beside its name (Figma, Série 16). The events stay in the corner of the toasts, top right.
///
/// In a VR headset, the HUD is on the screens of the cockpit (Figma, Série 17): its parts are given the
/// surface of their screen (engine::OnSurface), the pilot on HudShip, the other pilots on HudAux, the ping on
/// HudNav with the wave, the boss and the kind of game, which the screen does not show. The HudSystem places
/// them on their screens while the surfaces are apart.
/// @param maxLives Lives at the start of the game: one picture each.
/// @return The entity of the Hud.
ecs::entity_t openHud(ecs::Ecs &ecs, engine::SceneId scene, const HudLook &look, std::uint8_t maxLives);

} // namespace rtype::client
