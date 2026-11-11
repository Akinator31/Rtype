#pragma once

#include "Game/GameSessionSystem.hpp"
#include "Game/Hud.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Layout.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/game/Snapshot.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace rtype::client
{

/// Plays the HUD of the game (see openHud()). Each frame, it stretches its shadows across the screen, then
/// writes the players of the latest snapshot of the session: the lives of the local player lit for those
/// left, its slot, its name online, its score, its hit points and, with two weapons, the one in hand then
/// the one put away; the slot, the name online and the score of each other
/// player, their lines piled up from the ping; and the ping, with as many signal bars lit as it is short, or
/// nothing offline. With the voice of the session (GameSession::voiceView()), each pilot shows the icon of
/// its voice (voiceIconOf()), and the name of another pilot who talks lights up.
///
/// While the surfaces are apart (engine::Metrics::surfacesApart(), a VR headset shows the game), the HUD is
/// on the screens of the cockpit (Figma, Série 17), larger (CockpitHud): the pilot on the ship screen, the
/// ping on the navigation screen with the wave, the alert of a boss and the kind of game, the other pilots on
/// the screens of the sides. The shadows, which belong to the screen, are hidden.
class HudSystem : public ecs::System
{
    const GameSessionSystem &_sessions;
    const engine::Metrics &_metrics;
    ecs::ComponentCache<Hud> &_huds;
    ecs::ComponentCache<engine::Anchor> &_anchors;
    ecs::ComponentCache<engine::Text> &_texts;
    ecs::ComponentCache<engine::Sprite> &_sprites;
    ecs::ComponentCache<engine::Box> &_boxes;
    ecs::ComponentCache<engine::Opacity> &_opacities;
    ecs::ComponentCache<Replicated> &_replicated;
    ecs::ComponentCache<ecs::Position> &_positions;

    /// Gives the parts of the HUD their sizes, on the screen or on the screens of the cockpit, and their
    /// anchors on the screen, which the cockpit replaces.
    void applyScale(const Hud &hud, const HudScale &scale);
    /// Stretches the shadows across the screen; hides them in the cockpit.
    void showShadows(const Hud &hud, bool cockpit);
    void showPilot(const Hud &hud, std::uint8_t slot, const std::map<std::uint8_t, std::string> &names,
                   const std::optional<VoiceView> &voice);
    void showHealth(const Hud &hud);
    /// The names of the two weapons of the local player, the one in hand first, right of its hit points;
    /// nothing with one weapon.
    void showWeapons(const Hud &hud, bool cockpit);
    void showAllies(const Hud &hud, std::uint8_t local, const std::map<std::uint8_t, std::string> &names,
                    const std::optional<VoiceView> &voice);
    /// Shows an icon of the voice, or hides it for VoiceIcon::None.
    void showVoiceIcon(const Hud &hud, ecs::entity_t entity, VoiceIcon icon);
    void showPing(const Hud &hud, std::optional<std::chrono::milliseconds> ping);
    /// Writes the parts of the screens of the cockpit only, empty on the screen: the wave, the alert of a
    /// boss, the kind of game and the title of the other pilots.
    void showCockpitOnly(const Hud &hud, bool cockpit);
    /// Draws the radar of the hologram from the entities of the game; hides it on the screen.
    void showRadar(const Hud &hud, bool cockpit, std::uint8_t local);
    /// Places every part of the HUD on its screen of the cockpit.
    void placeInCockpit(const Hud &hud);
    /// Puts the anchor of an entity of the HUD at a point of its surface, by its centre `centre` under the
    /// top, as the Figma design measures it.
    void pin(ecs::entity_t entity, engine::AnchorPoint point, float x, float centre, float height);
    /// Puts the offset of the anchor of an entity of the HUD.
    void place(ecs::entity_t entity, float x, float y);
    /// Places an entity of the HUD anchored at the bottom of the screen by its centre, `centre` above the
    /// bottom.
    void placeAbove(ecs::entity_t entity, float x, float centre, float height);
    /// The width of a text of the HUD.
    [[nodiscard]] float measureWidth(ecs::entity_t entity) const;
    /// Whether a boss is in the game.
    [[nodiscard]] bool bossIsThere() const;

public:
    /// Signal bars lit under each of these pings, in milliseconds; one at least.
    static constexpr std::chrono::milliseconds FourBars{25};
    static constexpr std::chrono::milliseconds ThreeBars{60};
    static constexpr std::chrono::milliseconds TwoBars{120};

    HudSystem(ecs::ComponentCaches &caches, const GameSessionSystem &sessions,
              const engine::Metrics &metrics);

    void update(float elapsed) override;
};

/// The score of a player as the HUD writes it: seven digits.
[[nodiscard]] std::string formatScore(std::uint32_t score);

/// The experience of a player as the HUD writes it, "+340 XP  x3" with its streak from 2 waves: empty while
/// it has none, as offline.
[[nodiscard]] std::string formatXp(const game::PlayerState &player);

/// The wave as the navigation screen of the cockpit writes it: "03 / 08" with the number of waves, "03"
/// without one (an endless game).
[[nodiscard]] std::string formatWave(std::uint16_t wave, std::optional<std::uint16_t> count);

/// The signal bars lit for a ping, from 1 to 4.
[[nodiscard]] std::size_t countSignalBars(std::chrono::milliseconds ping);

} // namespace rtype::client
