#pragma once

#include "Game/ResultsScreen.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Layout.hpp"

namespace rtype::client
{

/// Plays the results screens (see openResultsScreen()), and their sounds (engine::SoundEffect), in the time
/// of the screen:
///
/// 1. the veil and the title fade in, with the sound of the game over;
/// 2. the figures land one after the other: each pops from larger, with a thump;
/// 3. the experience of the game counts up from 0, ticking higher and higher, while the bar of the level of
///    the account fills from the experience before the game to the one after; each level gained pops
///    LEVEL UP, with its fanfare, and the bar starts again from its left end. An achievement sound once all
///    is counted.
class ResultsSystem : public ecs::System
{
    ecs::Ecs &_ecs;
    ecs::ComponentCache<ResultsScreen> &_screens;
    ecs::ComponentCache<engine::Opacity> &_opacities;
    ecs::ComponentCache<engine::Text> &_texts;
    ecs::ComponentCache<engine::Box> &_boxes;
    ecs::ComponentCache<engine::Anchor> &_anchors;

    void fade(const ResultsScreen &screen);
    void land(ResultsScreen &screen);
    void count(ResultsScreen &screen, float elapsed);
    /// Writes the level of the account and lights its bar, for this experience in all.
    void showLevel(const ResultsScreen &screen, std::uint64_t xp);
    void setOpacity(ecs::entity_t entity, float opacity);
    void pop(ecs::entity_t entity, float scale);
    void play(ecs::entity_t entity, audio::SoundId sound, float volume, float pitch = 1.0f);

public:
    /// The veil and the title fade in in this time.
    static constexpr float TitleIn = 0.5f;
    /// The first figure lands at this time, the others one by one after it.
    static constexpr float FiguresDelay = 0.8f;
    static constexpr float FiguresGap = 0.35f;
    static constexpr float FigureIn = 0.2f;
    /// The experience starts counting at this time, and is counted in this time.
    static constexpr float CountDelay = 2.4f;
    static constexpr float CountDuration = 1.8f;
    /// The fastest the counting ticks, and how much higher the last tick is than the first.
    static constexpr float TickInterval = 0.05f;
    static constexpr float TickRise = 0.6f;
    /// A figure, the experience counted, or LEVEL UP pops from this scale.
    static constexpr float PopScale = 1.35f;
    static constexpr float PopDuration = 0.3f;

    static constexpr float GameOverVolume = 0.9f;
    static constexpr float StatVolume = 0.7f;
    static constexpr float TickVolume = 0.35f;
    static constexpr float CountedVolume = 0.7f;
    static constexpr float LevelUpVolume = 1.0f;

    ResultsSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs);

    void update(float elapsed) override;
};

} // namespace rtype::client
