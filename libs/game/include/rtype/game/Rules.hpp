#pragma once

#include <cstdint>

namespace rtype::game
{

/// The numbers of the gameplay, in one place to tune them: speeds in game units per second, durations in
/// ticks (TickRate per second). A Simulation is made with its rules, so that the tests can use their own.
struct Rules {
    /// Lives of each player at the start.
    std::uint8_t lives = 3;

    /// Speed of a ship, in every direction (diagonals included).
    float shipSpeed = 600.0f;
    /// Half the size of a ship: it stays entirely in the game area. The proportions of the hull of its model,
    /// as the client draws it.
    float shipHalfWidth = 84.0f;
    float shipHalfHeight = 49.0f;
    /// How far a ship turns its nose, up or down, while its player holds AimUp or AimDown, in degrees: it
    /// fires that way. Its hitbox does not turn.
    float aimAngle = 30.0f;

    /// A missile speeds up by this fraction of its launch speed per tick, up to missileMaxBoost times it.
    float missileBoostPerTick = 0.02f;
    float missileMaxBoost = 3.0f;
    /// A plasma projectile goes up then down at this vertical speed, switching every plasmaZigzagInterval
    /// ticks.
    float plasmaZigzagSpeed = 200.0f;
    std::uint32_t plasmaZigzagInterval = 20;

    /// Ticks between the loss of a ship and the next one, if the player has lives left.
    std::uint32_t respawnDelay = 180;
    /// Ticks a new ship cannot be hit.
    std::uint32_t invulnerableTicks = 120;
    /// Ticks a ship that was hit, and survived, cannot be hit again.
    std::uint32_t hitInvulnerableTicks = 60;

    /// Ticks before the first wave, once a player is in the game.
    std::uint32_t firstWaveDelay = 120;
    /// Ticks between the end of a wave and the start of the next one.
    std::uint32_t wavePause = 180;

    /// Online mode: the waves are drawn at random among all of them, one after the other, until every player
    /// is out of lives, and the players earn experience (Progression). Off: wave n is a wave of difficulty n,
    /// up to the final boss.
    bool endless = false;

    /// Offline story mode: when the last ship of the last player is lost against the final boss, allies come
    /// to the rescue instead of the defeat (RescueStage, RescueSystem). Off for the games online.
    bool rescue = false;
    /// Ticks of the voice the client plays when the rescue starts: the allies come when it ends. Keep it the
    /// length of the file of RTYPE_SOUND_RESCUE_VOICE in the client.
    std::uint32_t rescueVoiceTicks = 360;
    /// Ticks the allies and the new ship of the player take to fly in.
    std::uint32_t rescueArrivalTicks = 150;
    /// Ticks the allies shoot at the shogun before it strikes back.
    std::uint32_t rescueAssaultTicks = 300;
    /// Ticks of the combined attack, until the shogun falls.
    std::uint32_t rescueCombinedTicks = 150;
    /// Ticks between the fall of the shogun and the victory.
    std::uint32_t rescueVictoryDelay = 180;

    /// Where the items of the waves appear, at random: from itemMinX to the right edge less itemMargin, and
    /// itemMargin away from the top and the bottom.
    float itemMinX = 640.0f;
    float itemMargin = 120.0f;
    /// Ticks an item takes to appear, while the client plays its animation: it cannot be picked up before.
    std::uint32_t itemAppearTicks = 30;
    /// Ticks the items of a wave appear over, at least, when its enemies come faster.
    std::uint32_t itemMinWindow = 120;

    /// Seed of the random numbers of a game: the same seed and the same inputs give the same game.
    std::uint32_t seed = 1;

    /// How far outside the game area an entity that leaves it is removed, in game units.
    float areaMargin = 128.0f;
};

} // namespace rtype::game
