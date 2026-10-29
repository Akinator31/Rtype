#pragma once

#include "Game/DeathScreen.hpp"
#include "Game/GameSession.hpp"
#include "Game/GameSessionSystem.hpp"
#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/render/ResourceManager.hpp"

#include <functional>
#include <memory>
#include <optional>

namespace rtype::client
{

/// Names of the scenes of a game.
inline constexpr const char *GameScene = "game";
inline constexpr const char *PauseScene = "pause";

/// The scenes of a game (ADR 0015): the game scene, which plays a GameSession through the GameSessionSystem,
/// and the pause menu over it.
///
/// - The game scene shows the starry sky, the world of the session (the ships of the players and the Bydos
///   in 3D, through an orthographic camera: what is drawn is what collides), and the HUD (openHud()): the
///   lives and score of the local player, the score of the others, and the ping online.
/// - The events, in the corner of the toasts: a Bydo shot down by the local player, and the ship of a player
///   shot down, the local one or an ally.
/// - A banner across the screen when a wave starts, with its number.
/// - When the ship of the local player is lost with lives left, the death screen (openDeathScreen()): the red
///   banner "SHIP DESTROYED" with the lives left in its band, and the countdown to the next ship.
/// - Escape (or Start on a gamepad) opens the pause menu: Resume, or Leave the game. Offline, the game stops
///   while it is open.
/// - The game ends when the player leaves it or when its session is over: what to do then is given to play(),
///   back to the main menu by default.
///
/// @code
/// auto &sessions = ecs.registerSystem<GameSessionSystem>(ecs, engine.getScenes(), engine.getControls());
/// GameScenes games(engine, resources, audio, metrics, style, sessions, translations);
/// games.play(std::make_unique<LocalSession>()); // "Play" > "Solo"
/// @endcode
class GameScenes
{
    engine::Engine &_engine;
    const engine::Metrics &_metrics;
    const engine::InterfaceStyle &_style;
    GameSessionSystem &_sessions;
    localization::Localization &_translations;
    /// The game scene open, and the look of its death screen.
    std::optional<engine::SceneId> _scene;
    DeathScreenLook _deathLook;
    /// The session of the next game scene opened, and what to do when it ends.
    std::unique_ptr<GameSession> _next;
    std::function<void()> _nextEnd;
    /// What to do when the game played ends.
    std::function<void()> _onEnd;

    /// The game ends: once.
    void end();

    void build(engine::SceneId scene, render::ResourceManager &resources, audio::AudioPlayer &audio,
               const engine::Metrics &metrics, const engine::InterfaceStyle &style);
    /// Shows the event of a ship killed, if it is one: a Bydo killed by the local player, or a player ship.
    void showKill(const Kill &kill);
    void showWave(std::uint16_t wave);
    /// Shows the death screen of the local player, unless its ship was its last life.
    void showDeath();

public:
    /// Declares the scenes. Everything given must outlive the engine.
    GameScenes(engine::Engine &engine, render::ResourceManager &resources, audio::AudioPlayer &audio,
               const engine::Metrics &metrics, const engine::InterfaceStyle &style,
               GameSessionSystem &sessions, localization::Localization &translations);

    GameScenes(const GameScenes &) = delete;
    GameScenes &operator=(const GameScenes &) = delete;

    /// Starts a game with this session: the game scene replaces every scene at the next update of the engine.
    /// @param onEnd Called once when the player leaves the game or its session is over. Empty: back to the
    /// main menu.
    void play(std::unique_ptr<GameSession> session, std::function<void()> onEnd = {});
};

} // namespace rtype::client
