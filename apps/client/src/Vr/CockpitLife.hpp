#pragma once

#include "Vr/CockpitLook.hpp"
#include "rtype/render/Types.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace rtype::client
{

/// The materials of the cockpit that light up with the game, besides its screens (CockpitScreens): the cyan
/// and the orange glows along its frame and on ScreenFX, and the canopy, which has no light of its own.
inline constexpr std::string_view CockpitCyanGlow = "CP_GlowCyan";
inline constexpr std::string_view CockpitOrangeGlow = "CP_GlowOrange";
inline constexpr std::string_view CockpitCanopy = "CP_Glass";

/// What the game says of the ship of the player each frame, for the cockpit (GameSessionSystem).
struct CockpitFeel {
    /// A game is played: the player sits in the cockpit.
    bool playing = false;
    /// The hit points of their ship, 0 while it is not there, and the most it had since it appeared.
    std::uint8_t health = 0;
    std::uint8_t maxHealth = 0;
    /// A boss is in the game.
    bool boss = false;
    /// The colour of the player (playerColor()), which the canopy glows at when the game starts.
    render::Color pilot = render::colors::White;
};

/// The lights of the cockpit for a frame: the colour each material emits (render::Renderer::
/// setModelMaterialGlow()), white for the light of the artist as it is, and how bright the whole cockpit is.
struct CockpitLights {
    /// The cockpit, from black (0) to lit by the scene (1): it fades in when a game starts.
    float brightness = 1.0f;
    /// Each screen, in the order of CockpitScreens.
    std::array<render::Color, CockpitScreens.size()> screens{render::colors::White, render::colors::White,
                                                             render::colors::White, render::colors::White};
    render::Color cyan = render::colors::White;
    render::Color orange = render::colors::White;
    /// The canopy: black, no light, but when the game starts.
    render::Color canopy = render::colors::Black;
};

/// How the cockpit starts when a game starts: it fades in from black, then its screens light up one after
/// the other, while the canopy glows at the colour of the player and fades.
inline constexpr float CockpitBootFade = 0.8f;
inline constexpr float CockpitFirstScreen = 0.6f;
inline constexpr float CockpitScreenGap = 0.25f;
inline constexpr float CockpitScreenLighting = 0.15f;
inline constexpr float CanopyGlowEnd = 3.0f;
/// How bright the canopy glows at the start, from 0 to 1 of the colour of the player.
inline constexpr float CanopyGlow = 0.35f;
/// How long the red of a hit lasts, in seconds.
inline constexpr float HitFlash = 0.5f;
/// The ship is low on life at a third of its hit points or less: the lights pulse red, the alarm repeats.
inline constexpr float LowLife = 1.0f / 3.0f;
/// How often the lights pulse while the ship is low on life, and how often the alarm sounds, in seconds.
inline constexpr float LowLifePulse = 1.0f;
inline constexpr float LowLifeAlarm = 1.5f;
/// How often the orange glows pulse while a boss is there, in seconds.
inline constexpr float BossPulse = 2.0f;
/// How long the screens flicker when the ship is destroyed, in seconds, and how bright they stay after,
/// until it comes back.
inline constexpr float DeathFlicker = 1.5f;
inline constexpr float DeadScreens = 0.15f;
/// The red of the alerts: the orange glows multiplied by it turn red.
inline constexpr render::Color AlertRed{255, 40, 30, 255};

/// The cockpit alive (docs/architecture/vr.md#the-cockpit-alive): its lights react to the game, it never
/// moves. Red pulses and an alarm when the ship of the player is hit or low on life, the orange glows that
/// warn of a boss, the screens that flicker at its death, and the start of a game: the cockpit fades in from
/// black, its screens light up one after the other, the canopy glows at the colour of the player. It reads
/// what the game shows, nothing else.
/// @code
/// life.update(elapsed, {.playing = sessions.running(), .health = sessions.localHealth(), ...});
/// vrView.update(elapsed, {..., .cockpit = life.lights()});
/// if (life.takeAlarm()) { audio.play(alarm); }
/// @endcode
class CockpitLife
{
    std::optional<CockpitFeel> _previous;
    /// Time since the game started, since the last hit and since the ship was destroyed, in seconds; empty
    /// before the first.
    float _sinceStart = 0.0f;
    std::optional<float> _sinceHit;
    std::optional<float> _sinceDeath;
    /// Time while the ship is low on life, for the pulse, and until the next alarm.
    float _lowTime = 0.0f;
    float _nextAlarm = 0.0f;
    /// Time while a boss is there, for its pulse.
    float _bossTime = 0.0f;
    bool _alarm = false;
    CockpitLights _lights;

    /// The orange and the cyan glows for the state of the ship.
    void lightGlows(const CockpitFeel &feel);
    void lightScreens();
    void lightCanopy(const CockpitFeel &feel);

public:
    /// Once per frame: what changed since the previous frame. Out of a game, the lights are those of the
    /// artist and nothing sounds.
    /// @param elapsed Time since the previous frame, in seconds.
    void update(float elapsed, const CockpitFeel &feel);

    /// The lights for this frame.
    [[nodiscard]] const CockpitLights &lights() const { return _lights; }
    /// An alarm must sound this frame: once at a hit, then again and again while the ship is low on life.
    /// The call clears it.
    [[nodiscard]] bool takeAlarm();
};

} // namespace rtype::client
