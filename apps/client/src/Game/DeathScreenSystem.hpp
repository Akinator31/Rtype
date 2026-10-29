#pragma once

#include "Game/DeathScreen.hpp"
#include "Game/GameSessionSystem.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"

#include <cstdint>

namespace rtype::client
{

/// Plays the death screens (see openDeathScreen()), in the time of the game: the countdown follows the ticks
/// of the session, so it stops while the game is paused. Each frame, it writes the seconds left before the
/// next ship in the ring, which runs out with them, and pops each new second; the veil fades in at once, the
/// rest once the banner has opened, then the life just lost goes out. At the tick of the next ship,
/// everything fades out and is destroyed.
class DeathScreenSystem : public ecs::System
{
    ecs::Ecs &_ecs;
    const GameSessionSystem &_sessions;
    ecs::ComponentCache<DeathScreen> &_screens;
    ecs::ComponentCache<engine::Opacity> &_opacities;
    ecs::ComponentCache<engine::Text> &_texts;
    ecs::ComponentCache<engine::Ring> &_rings;

    /// Writes the seconds left in the countdown and its ring.
    void showCountdown(const DeathScreen &screen, std::int64_t ticksLeft);
    void fade(const DeathScreen &screen);
    void setOpacity(ecs::entity_t entity, float opacity);
    void close(ecs::entity_t entity, const DeathScreen &screen);

public:
    /// Time the veil takes to fade in, in seconds.
    static constexpr float VeilIn = 0.3f;
    /// The rest appears once the band of the banner is open, in this time.
    static constexpr float ContentDelay = 0.6f;
    static constexpr float ContentIn = 0.4f;
    /// The life just lost goes out once it was seen, in this time.
    static constexpr float LifeOutDelay = 1.2f;
    static constexpr float LifeOut = 0.6f;
    /// Time the screen takes to fade out once the next ship is there.
    static constexpr float FadeOut = 0.3f;
    /// A new second pops: it appears larger, then shrinks to its size.
    static constexpr float PopScale = 1.3f;
    static constexpr float PopDuration = 0.25f;

    DeathScreenSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, const GameSessionSystem &sessions);

    void update(float elapsed) override;
};

} // namespace rtype::client
