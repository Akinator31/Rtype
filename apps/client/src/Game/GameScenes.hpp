#pragma once

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

namespace rtype::client
{

/// Names of the scenes of a game.
inline constexpr const char *GameScene = "game";
inline constexpr const char *PauseScene = "pause";

/// The scenes of a game (ADR 0015): the game scene, which plays a GameSession through the GameSessionSystem,
/// and the pause menu over it.
///
/// - The game scene shows the starry sky, the world of the session (the ships of the players and the Bydos
///   in 3D, through an orthographic camera: what is drawn is what collides), and a HUD with the lives and
///   score of each player, in its colour.
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
    GameSessionSystem &_sessions;
    localization::Localization &_translations;
    /// The session of the next game scene opened, and what to do when it ends.
    std::unique_ptr<GameSession> _next;
    std::function<void()> _nextEnd;
    /// What to do when the game played ends.
    std::function<void()> _onEnd;

    /// The game ends: once.
    void end();

    void build(engine::SceneId scene, render::ResourceManager &resources, audio::AudioPlayer &audio,
               const engine::Metrics &metrics,
               const engine::InterfaceStyle &style);
    void buildHud(engine::SceneId scene, const engine::InterfaceStyle &style);

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
