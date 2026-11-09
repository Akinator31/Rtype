#pragma once

#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/render/ResourceManager.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rtype::client
{

/// What the local player did in a game online that went to its end, from its last game::PlayerState.
struct GameResults {
    std::uint16_t wavesCleared = 0;
    std::uint32_t kills = 0;
    std::uint16_t bestStreak = 0;
    std::uint32_t score = 0;
    /// Experience earned in the game.
    std::uint32_t xp = 0;

    friend bool operator==(const GameResults &, const GameResults &) = default;
};

/// The sounds of the results screen.
struct ResultsSounds {
    audio::SoundId gameOver;
    audio::SoundId stat;
    audio::SoundId tick;
    audio::SoundId counted;
    audio::SoundId levelUp;
};

/// The look of the results screen: its fonts, its sounds, and its captions in the language of the player.
struct ResultsLook {
    render::FontId captionFont;
    render::FontId valueFont;
    ResultsSounds sounds;
    /// "THE R-TYPE SECTION HAS FALLEN", in chrome at the top.
    std::string title;
    /// Under the figures: the waves cleared, the Bydos shot down, the best streak, the score.
    std::string waves;
    std::string kills;
    std::string streak;
    std::string score;
    /// Under the experience earned.
    std::string xp;
    /// "LEVEL", before the number of the level.
    std::string level;
    /// Pops when the level goes up.
    std::string levelUp;
    /// Under the experience of a guest, who keeps none: empty for none, on a server without accounts.
    std::string guest;
};

/// Length of the bar of the level, in game units: the ResultsSystem lights it from its left end.
inline constexpr float ResultsBarWidth = 900.0f;

/// The results screen showing, made of entities of its scene: the ResultsSystem plays it.
struct ResultsScreen {
    GameResults game;
    /// The experience of the account before the game, none for a guest, and after it: guessed as the one
    /// before and the experience of the game until the master tells it (setAccountXp()).
    std::optional<std::uint64_t> before;
    std::uint64_t after = 0;
    ResultsSounds sounds;
    std::string levelCaption;

    ecs::entity_t veil = 0;
    ecs::entity_t title = 0;
    /// The figures, each with its caption under it, in the order they land.
    std::vector<ecs::entity_t> figures;
    std::vector<ecs::entity_t> captions;
    /// "+820 XP", counting up, and its caption.
    ecs::entity_t xp = 0;
    ecs::entity_t xpCaption = 0;
    /// For an account: "LEVEL 4", the bar of the level, behind and lit, the experience into the level, and
    /// "LEVEL UP", hidden until the level goes up.
    ecs::entity_t level = 0;
    ecs::entity_t barBack = 0;
    ecs::entity_t barFill = 0;
    ecs::entity_t levelXp = 0;
    ecs::entity_t levelUp = 0;
    /// What fades in as the experience starts counting: the experience, and the level but LEVEL UP.
    std::vector<ecs::entity_t> experience;
    /// For a guest, the hint that it keeps nothing, once all is counted.
    std::optional<ecs::entity_t> guest;

    /// Seconds since it opened, and whether the sound of the game over played.
    float time = 0.0f;
    bool opened = false;
    /// Figures landed so far.
    std::size_t landed = 0;
    /// The experience of the game counted so far, the level shown, and the time since the last tick.
    std::uint32_t xpShown = 0;
    std::uint32_t levelShown = 1;
    float sinceTick = 0.0f;
    bool counted = false;
};

/// Opens the results screen of a game online in a scene (docs/architecture/client-game.md, "Results
/// screen"): the fall of the R-Type section in chrome, then the waves cleared, the Bydos shot down, the best
/// streak and the score, landing one after the other, then the experience earned counting up while the bar of
/// the level of the account fills, LEVEL UP each time it goes up. A guest sees what it earned, and that it
/// keeps nothing.
/// @param accountXp The experience of the account before the game; none for a guest.
/// @return The entity of the ResultsScreen.
ecs::entity_t openResultsScreen(ecs::Ecs &ecs, engine::SceneId scene, const engine::Metrics &metrics,
                                const ResultsLook &look, const GameResults &game,
                                std::optional<std::uint64_t> accountXp);

/// The master told the experience of the account after the game (PROGRESS): the bar fills up to it.
void setAccountXp(ResultsScreen &screen, std::uint64_t xp);

} // namespace rtype::client
