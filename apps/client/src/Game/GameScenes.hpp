#pragma once

#include "Game/GameSession.hpp"
#include "Game/GameSessionSystem.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/render/ResourceManager.hpp"

#include <memory>

namespace rtype::client
{

/// Names of the scenes of a game.
inline constexpr const char *GameScene = "game";
inline constexpr const char *PauseScene = "pause";

/// The scenes of a game (ADR 0015): the game scene, which plays a GameSession through the GameSessionSystem,
/// and the pause menu over it.
///
/// - The game scene shows the starry sky, the world of the session, and a HUD with the lives and score of
///   each player, in its colour.
/// - Escape (or Start on a gamepad) opens the pause menu: Resume, or back to the main menu. Offline, the game
///   stops while it is open.
///
/// @code
/// auto &sessions = ecs.registerSystem<GameSessionSystem>(ecs, engine.getScenes(), engine.getControls());
/// GameScenes games(engine, resources, metrics, style, sessions);
/// games.play(std::make_unique<LocalSession>()); // "Play" > "Solo"
/// @endcode
class GameScenes
{
    engine::Engine &_engine;
    GameSessionSystem &_sessions;
    /// The session of the next game scene opened.
    std::unique_ptr<GameSession> _next;

    void build(engine::SceneId scene, render::ResourceManager &resources,
               const engine::InterfaceStyle &style);

public:
    /// Declares the scenes. Everything given must outlive the engine.
    GameScenes(engine::Engine &engine, render::ResourceManager &resources, const engine::Metrics &metrics,
               const engine::InterfaceStyle &style, GameSessionSystem &sessions);

    GameScenes(const GameScenes &) = delete;
    GameScenes &operator=(const GameScenes &) = delete;

    /// Starts a game with this session: the game scene replaces every scene at the next update of the engine.
    void play(std::unique_ptr<GameSession> session);
};

} // namespace rtype::client
