#pragma once

#include "Discord/DiscordFriends.hpp"
#include "Game/DeathScreen.hpp"
#include "Game/GameRecorder.hpp"
#include "Game/GameSession.hpp"
#include "Game/GameSessionSystem.hpp"
#include "Game/ResultsScreen.hpp"
#include "ShipSelector/ShipSelection.hpp"
#include "Voice/VoiceIcons.hpp"
#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/game/Item/ItemDefinition.hpp"
#include "rtype/game/Ship/ShipDefinition.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/render/ResourceManager.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace rtype::client
{

/// Names of the scenes of a game.
inline constexpr const char *GameScene = "game";
inline constexpr const char *PauseScene = "pause";
/// Name of the results of a game online that went to its end.
inline constexpr const char *ResultsScene = "game_results";

class RemoteSession;

/// The scenes of a game (ADR 0015): the game scene, which plays a GameSession through the GameSessionSystem,
/// and the pause menu over it.
///
/// - The game scene shows the starry sky, the world of the session (the ships of the players and the Bydos
///   in 3D, through an orthographic camera: what is drawn is what collides; the local player flies the ship
///   equipped in the hangar), and the HUD (openHud()): the
///   lives and score of the local player, the score of the others, and the ping online.
/// - The events, in the corner of the toasts: a Bydo shot down by the local player, and the ship of a player
///   shot down, the local one or an ally.
/// - A banner across the screen when a wave starts, with its number, and its music if it has one
///   (WaveDefinition::music); the music stops when the game ends.
/// - When the ship of the local player is lost with lives left, the death screen (openDeathScreen()): the red
///   banner "SHIP DESTROYED" with the lives left in its band, and the countdown to the next ship.
/// - Offline, the rescue against the final boss (game::RescueStage): when the last ship is lost against the
///   shogun, its voice plays, then a banner greets the allies. While the shogun dashes through them, the
///   camera closes up on it and follows it (RescueCamera), each ship its blade goes through shows the cut and
///   shakes the camera; then a blinking prompt asks for Space, the combined attack, during which the camera
///   pushes in slowly on the shogun, rumbling, until its fall shakes it hard, then pulls back.
/// - Escape (or Start on a gamepad) opens the pause menu (Figma, Série 16): Resume, or Leave the game, and
///   online with a voice, the voice of each other player: its volume for this client (Left, Right) and
///   muted (Confirm), with the icon of its voice. Online and connected to Discord, the Discord friends: those
///   asking to join the game (Accept), then those online (Invite). They are kept for the next games when the
///   menu closes (VoicePreferences::players). Offline, the game stops while it is open.
/// - The game ends when the player leaves it or when its session is over: what to do then is given to play(),
///   back to the main menu by default.
/// - Online, a game that went to its end keeps what the local player did in it (results()), which
///   showResults() shows on the results screen (openResultsScreen()) over the sky of the game: the figures of
///   the game, the experience earned and the level of the account, with their sounds.
///
/// @code
/// auto &sessions = ecs.registerSystem<GameSessionSystem>(ecs, engine.getScenes(), engine.getControls());
/// GameScenes games(engine, resources, audio, metrics, style, sessions, translations, ships, config.ships);
/// games.play(std::make_unique<LocalSession>()); // "Play" > "Solo"
/// @endcode
class GameScenes
{
    engine::Engine &_engine;
    audio::AudioPlayer &_audio;
    const engine::Metrics &_metrics;
    const engine::InterfaceStyle &_style;
    GameSessionSystem &_sessions;
    localization::Localization &_translations;
    const ShipSelection &_ships;
    /// The ships of the configuration, whose models the Bydos fly.
    const std::vector<game::ShipDefinition> &_shipDefinitions;
    const std::vector<game::ItemDefinition> &_itemDefinitions;
    GameRecorder _recorder;
    /// The game scene open, and the look of its death screen.
    std::optional<engine::SceneId> _scene;
    DeathScreenLook _deathLook;
    /// The prompt of the combined attack, while it shows.
    std::optional<ecs::EntityRef> _prompt;
    /// The camera of the game scene open, with its RescueCamera.
    std::optional<ecs::EntityRef> _camera;
    /// The session of the next game scene opened, and what to do when it ends.
    std::unique_ptr<GameSession> _next;
    std::function<void()> _nextEnd;
    /// What to do when the game played ends.
    std::function<void()> _onEnd;
    /// The icons of the voice, loaded with the game scene.
    VoiceIconTextures _voiceIcons{};
    /// Online, what the local player did in the game played, once it went to its end.
    std::optional<GameResults> _results;
    /// The results shown: the experience of the account before the game and after it, once the master told
    /// it, whether a guest could sign in, and what Continue does.
    std::optional<std::uint64_t> _xpBefore;
    std::optional<std::uint64_t> _xpAfter;
    bool _canSignIn = false;
    std::function<void()> _onContinue;
    /// The ResultsScreen of the results scene open.
    std::optional<ecs::EntityRef> _resultsScreen;

    /// The game ends: once.
    void end();
    /// The session played, if it is online.
    [[nodiscard]] RemoteSession *onlineSession() const;
    /// The pause menu: the game on the left, the voices of the players on the right.
    void buildPause(engine::SceneId scene);
    /// The voice of each other player, in a column of the pause menu, if the game has a voice and players.
    void addPlayerVoices(engine::InterfaceBuilder &interface, float left);
    /// The Discord friends of the pause menu of a game online, under the group of the game, from `top`: those
    /// asking to join first (Accept), then those online (Invite).
    void addFriends(engine::InterfaceBuilder &interface, float left, float top);
    /// The Discord friends of the player, none until given.
    discord::Friends *_friends = nullptr;
    /// Keeps the choices made for the other players in the settings, and writes them.
    void keepPlayerVoices();
    /// Closes the pause menu.
    void resume();

    void build(engine::SceneId scene, render::ResourceManager &resources, audio::AudioPlayer &audio,
               const engine::Metrics &metrics, const engine::InterfaceStyle &style);
    /// Tells the recorder about a ship killed, during a session.
    void recordKill(const Kill &kill);
    /// Tells the recorder that the game ends, with the score of the local player.
    void recordEnd();
    /// Shows the event of a ship killed, if it is one: a Bydo killed by the local player, or a player ship.
    /// The fall of the shogun at the end of the rescue shakes the camera.
    void showKill(const Kill &kill);
    void showWave(std::uint16_t wave);
    /// Online, at the end of the game: keeps what the local player did in it, for showResults().
    void recordResults();
    void buildResults(engine::SceneId scene, render::ResourceManager &resources);
    /// Leaves the results: what showResults() was given, else back to the main menu. Once.
    void leaveResults();
    /// Plays the music of the wave started, a path of the assets, replacing the current one; nothing if
    /// empty.
    void playWaveMusic(const std::string &music);
    /// Shows the death screen of the local player, unless its ship was its last life.
    void showDeath();
    /// Shows a new stage of the rescue against the final boss: its voice, the banner of the allies, the
    /// prompt of the combined attack.
    void showRescue(game::RescueStage stage);
    /// Shakes the camera of the game as the blade of the shogun goes through a ship.
    void showStrike();
    /// Shakes the camera of the game scene open by a blow of a strength from 0 to 1 (shakeCamera()).
    void shakeGameCamera(float strength);
    /// Plays the voice of the rescue, if its file is in the assets.
    void playRescueVoice();
    /// The prompt of the combined attack, blinking under the middle of the screen until the player presses
    /// Fire.
    void openPrompt();
    void closePrompt();

public:
    /// The Discord friends of the player, invited from the pause menu of a game online. They outlive the
    /// scenes.
    void setFriends(discord::Friends *friends) { _friends = friends; }

    /// Declares the scenes. Everything given must outlive the engine.
    GameScenes(engine::Engine &engine, render::ResourceManager &resources, audio::AudioPlayer &audio,
               const engine::Metrics &metrics, const engine::InterfaceStyle &style,
               GameSessionSystem &sessions, localization::Localization &translations,
               const ShipSelection &ships, const std::vector<game::ShipDefinition> &shipDefinitions,
               const std::vector<game::ItemDefinition> &itemDefinitions);

    GameScenes(const GameScenes &) = delete;
    GameScenes &operator=(const GameScenes &) = delete;

    /// Starts a game with this session: the game scene replaces every scene at the next update of the engine.
    /// @param onEnd Called once when the player leaves the game or its session is over. Empty: back to the
    /// main menu.
    void play(std::unique_ptr<GameSession> session, std::function<void()> onEnd = {});

    /// Online, what the local player did in the game played, if it went to its end; none before, offline,
    /// or once the player left it.
    [[nodiscard]] const std::optional<GameResults> &results() const { return _results; }
    /// Shows the results of the game played, which went to its end (results()), in place of every scene.
    /// @param accountXp The experience of the account before the game; none for a guest.
    /// @param canSignIn A guest could have kept its experience by signing in: it is told so.
    /// @param onContinue Called once when the player leaves the results. Empty: back to the main menu.
    void showResults(std::optional<std::uint64_t> accountXp, bool canSignIn,
                     std::function<void()> onContinue);
    /// The master told the experience of the account after the game played (PROGRESS): its results show it.
    void setAccountXp(std::uint64_t xp);
};

} // namespace rtype::client
