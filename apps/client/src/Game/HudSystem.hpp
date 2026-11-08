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
/// left, its slot, its name online and its score; the slot, the name online and the score of each other
/// player, their lines piled up from the ping; and the ping, with as many signal bars lit as it is short, or
/// nothing offline. With the voice of the session (GameSession::voiceView()), each pilot shows the icon of
/// its voice (voiceIconOf()), and the name of another pilot who talks lights up.
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

    void stretchShadows(const Hud &hud);
    void showPilot(const Hud &hud, std::uint8_t slot, const std::map<std::uint8_t, std::string> &names,
                   const std::optional<VoiceView> &voice);
    void showAllies(const Hud &hud, std::uint8_t local, const std::map<std::uint8_t, std::string> &names,
                    const std::optional<VoiceView> &voice);
    /// Shows an icon of the voice, or hides it for VoiceIcon::None.
    void showVoiceIcon(const Hud &hud, ecs::entity_t entity, VoiceIcon icon);
    void showPing(const Hud &hud, std::optional<std::chrono::milliseconds> ping);
    /// Puts the offset of the anchor of an entity of the HUD.
    void place(ecs::entity_t entity, float x, float y);
    /// Places an entity of the HUD anchored at the bottom of the screen by its centre, `centre` above the
    /// bottom.
    void placeAbove(ecs::entity_t entity, float x, float centre, float height);
    /// The width of a text of the HUD.
    [[nodiscard]] float measureWidth(ecs::entity_t entity) const;

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

/// The signal bars lit for a ping, from 1 to 4.
[[nodiscard]] std::size_t countSignalBars(std::chrono::milliseconds ping);

} // namespace rtype::client
