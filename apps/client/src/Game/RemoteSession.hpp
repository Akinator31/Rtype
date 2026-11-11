#pragma once

#include "Game/GameSession.hpp"
#include "Game/InputClock.hpp"
#include "Game/Interpolation.hpp"
#include "Game/Prediction.hpp"
#include "Game/ShotPrediction.hpp"
#include "Voice/VoiceDevices.hpp"
#include "rtype/discord/DiscordVoice.hpp"
#include "rtype/game/Config/GameConfig.hpp"
#include "rtype/game/Messages.hpp"
#include "rtype/game/RemoteWorld.hpp"
#include "rtype/lobby/Messages.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/Session.hpp"
#include "rtype/network/UdpSocket.hpp"
#include "rtype/voice/Backend.hpp"
#include "rtype/voice/Devices.hpp"
#include "rtype/voice/GameVoice.hpp"
#include "rtype/voice/SharedPlayback.hpp"

#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rtype::client
{

/// Gives the voice of a game how the player talks: its mode (push-to-talk, voice activity or off), muted
/// and deafened by its keys, and its volumes. The same for the voice of the game (voice::GameVoice) and the
/// one of Discord (discord::DiscordVoice).
/// @param pushToTalk The push-to-talk key is held, or was released less than voice::PushToTalk::ReleaseDelay
///   ago.
/// @param forceMute Muted whatever the player chose (by an administrator, for instance): given here rather
///   than muted after, so that the microphone is not opened and closed again at every update.
template <typename Voice>
void applyVoice(Voice &voice, const VoiceDevices &devices, bool pushToTalk, bool forceMute = false)
{
    const VoicePreferences &preferences = devices.preferences;
    const bool activity = preferences.mode == voice::TalkMode::VoiceActivity;
    voice.setDeafened(devices.deafened);
    voice.setSelfMute(forceMute || preferences.mode == voice::TalkMode::Off || devices.muted ||
                      devices.deafened);
    voice.setVoiceActivity(activity ? std::optional<float>(preferences.threshold) : std::nullopt);
    voice.setTalking(activity || (preferences.mode == voice::TalkMode::PushToTalk && pushToTalk));
    voice.setInputVolume(preferences.inputVolume);
    voice.setOutputVolume(preferences.voiceVolume);
    voice.setProcessing(preferences.processing);
}

/// A game online: the session of the player with its game instance, over encrypted UDP
/// (docs/protocol/game.md), with what GAME_JOINED gave (ADR 0015).
///
/// @code
/// games.play(std::make_unique<RemoteSession>(loop, master.withPort(joined.udpPort), joined));
/// @endcode
///
/// - **Inputs**: once per tick (60 per second), it sends INPUT with the inputs of the last 8 ticks, newest
///   first, so that a lost datagram loses no input. Each one is stamped with the tick the game should apply
///   it at (section 9.2): first estimated from the latest snapshot and the round-trip time, then kept just
///   ahead of the game by the lead each snapshot reports (InputClock), a little faster or slower than 60
///   per second, or with a jump when they arrive too late.
/// - **Snapshots**: the parts received and the destructions go into a RemoteWorld, and each world received
///   into an Interpolation; takeSnapshots() gives, each frame, the world drawn 100 ms in the past, between
///   the two snapshots around that time: the entities move at the frame rate, not 30 times per second.
/// - **The ship of the player** is predicted: each input moves it at once, and each snapshot that has it
///   reconciles the prediction with the game (Prediction). It is drawn where it is predicted, and its shots
///   in its time (ShotPrediction), the rest where the interpolation draws it.
/// - **The voice** (section 9.4): it tells the game what carries the voice of the player (VOICE_STATE, at
///   the connection and at every change), sends its frames (VOICE), and keeps those of the others
///   (VOICE_FRAME) for takeVoiceFrames(), with what carries the voice of each player (VOICE_ROSTER). Given
///   VoiceDevices, it runs the voice of the game itself (voice::GameVoice): the player is on Backend::Game
///   if the voices can be played, the frames go through the GameVoice instead of takeVoiceFrames(), and it
///   talks as VoiceDevices says: in its mode, with its keys and volumes (applyVoice()). It tells the game
///   whether the player is muted, deafened or without a microphone (VOICE_STATUS, at every change), gives
///   the GameVoice what the others tell (VOICE_STATUSES), and hears each other player as the player chose
///   in the pause menu (VoicePreferences::players, under voiceKey()). A player muted by an administrator
///   has its microphone closed while it lasts; a game without the voice (`ACCEPTED`) runs none.
/// - **The voice of Discord**: given the Discord call of VoiceDevices, the secret of GAME_JOINED and the id
///   of the player, it joins the Discord lobby of the game once accepted (discord::DiscordVoice), hearing the
///   voice of the game meanwhile, its microphone closed: Discord opens it, and closing a microphone Discord
///   just opened stalls on some systems (a Bluetooth headset on PipeWire). Not connected within
///   DiscordJoinTimeout, the call is given up. Once the call is connected, the player is on Backend::Discord.
/// - **Mixed games** (docs/architecture/voice.md#mixed-games): on Discord, the GameVoice goes on beside the
///   DiscordVoice, both playing through one voice::SharedPlayback: each other player is heard from the
///   source the roster gives it, Discord for those on Discord, the game for the others, never both. The
///   voice of the player goes to the others on Discord through its call, and to those on the game as VOICE:
///   the GameVoice takes what Discord captures (DiscordVoice::capture()), and is muted while no other player
///   is on the game. If Discord gives no microphone for DiscordCaptureTimeout while the player talks, the
///   GameVoice opens the microphone of VoiceDevices instead, for the rest of the game. A call that drops
///   gives the player back to the voice of the game, and is joined again DiscordRetryDelay later, up to
///   MaxDiscordRetries times.
/// - **The end**: DISCONNECT from the game, a timeout, or GAME_OVER; endReason() tells the player why. The
///   snapshot taken after GAME_OVER has its outcome, and the players as the latest snapshot received has
///   them, their final scores, not those of 100 ms before.
///
/// The loop must be polled, by the game loop of the client.
class RemoteSession final : public GameSession, private voice::VoiceTransport
{
public:
    /// Inputs in each INPUT: the last ticks, so that a lost datagram loses none (docs/protocol/game.md, 9.2).
    static constexpr std::size_t InputHistory = game::InputRedundancy;
    /// Frames of the voice of the others kept until takeVoiceFrames(): one second of three players. Beyond,
    /// the oldest go.
    static constexpr std::size_t MaxVoiceFrames = 150;
    /// How long the call of Discord has to connect, the microphone of the game closed meanwhile; past it, the
    /// voice of the game takes over.
    static constexpr std::chrono::seconds DiscordJoinTimeout{10};
    /// How long after its call dropped the lobby of Discord is joined again, the voice of the game meanwhile.
    static constexpr std::chrono::seconds DiscordRetryDelay{5};
    /// The calls of Discord joined again in a game, after it dropped, before the voice of the game is kept.
    static constexpr int MaxDiscordRetries = 3;
    /// How long Discord may give no microphone while the player talks on it, in a mixed game, before the
    /// voice of the game opens one of its own.
    static constexpr std::chrono::seconds DiscordCaptureTimeout{2};

private:
    using Clock = network::ClientSession::Clock;

    network::UdpSocket _socket;
    network::Address _game;
    network::ClientSession _session;
    game::RemoteWorld _world;
    Interpolation _interpolation;
    Prediction _prediction;
    ShotPrediction _shots;
    InputClock _inputClock;
    /// The tick of the last snapshot whose input lead was given to the InputClock.
    std::uint32_t _leadTick = 0;
    /// The weapon of the ships of the players, from the definitions of the client: what its shots are
    /// predicted with, once the slot is known.
    std::optional<game::WeaponDefinition> _weapon;
    float _projectileHalfWidth = 0.0f;
    std::optional<game::AcceptedMessage> _accepted;
    /// The id of the ship of the player, 0 while it has none.
    std::uint32_t _shipId = 0;
    std::map<std::uint8_t, std::string> _names;
    /// The account of each player, by slot, 0 for a guest (`PLAYER_JOINED`).
    std::map<std::uint8_t, std::uint64_t> _accounts;
    /// The address of the server, as the player wrote it: the choices for the other players are kept under
    /// it.
    std::string _server;
    /// The id of the game on its master (GAME_JOINED).
    std::uint32_t _gameId = 0;

    game::PlayerInput _input;
    /// The inputs sent, newest first.
    std::deque<game::PlayerInput> _history;
    /// The tick stamped on the newest input sent.
    std::uint32_t _inputTick = 0;
    double _pending = 0.0;
    Clock::time_point _snapshotReceived;

    std::vector<game::DestroyedEntity> _destroyed;
    /// The wave WAVE_STARTED told, until the next snapshot taken.
    std::optional<std::uint16_t> _waveStarted;
    std::string _waveMusic;
    /// The outcome GAME_OVER told, until the next snapshot taken: the snapshots of the game do not carry it.
    std::optional<game::GameOutcome> _outcome;
    bool _over = false;
    std::string _endReason;

    /// What carries the voice of the player, told to the game once accepted.
    voice::Backend _voiceBackend = voice::Backend::None;
    game::VoiceRosterMessage _voiceRoster;
    /// What each player tells about its voice, from the latest VOICE_STATUSES.
    game::VoiceStatusesMessage _voiceStatuses;
    /// What the player told the game about its voice, once told.
    std::optional<voice::Status> _toldStatus;
    std::deque<game::VoiceFrameMessage> _voiceFrames;
    /// The voice of the game, with VoiceDevices. Last: destroyed first, while the session it sends through is
    /// still there.
    VoiceDevices *_devices = nullptr;
    voice::PushToTalk _pushToTalk;
    /// The playback of VoiceDevices, shared by the GameVoice and the DiscordVoice: before both.
    std::optional<voice::SharedPlayback> _playbacks;
    std::unique_ptr<voice::GameVoice> _voice;
    /// The voice of Discord, for a player with Discord in a game that has its lobby, until it is given up.
    std::unique_ptr<discord::DiscordVoice> _discord;
    /// The call of Discord is connected: the voice of the player goes through it, and through the GameVoice
    /// to the players on the game.
    bool _onDiscord = false;
    /// When the call of Discord was started: past DiscordJoinTimeout without connecting, it is given up.
    voice::TalkClock::time_point _discordSince;
    /// The call of Discord was connected once in this game: joining it again, the microphone of the game
    /// stays open meanwhile.
    bool _discordWasConnected = false;
    /// When the call of Discord is joined again, after it dropped; none while it is not to be.
    std::optional<voice::TalkClock::time_point> _discordRetryAt;
    int _discordRetries = 0;
    /// The GameVoice takes the microphone of Discord, not the one of VoiceDevices.
    bool _discordCapture = false;
    /// Discord gave no microphone while the player talked: the GameVoice keeps the one of VoiceDevices.
    bool _discordCaptureMissing = false;
    /// The samples Discord had captured, and when that last changed, while the player talks.
    std::uint64_t _discordCaptured = 0;
    voice::TalkClock::time_point _discordCapturedAt;

    void received(std::uint8_t type, std::span<const std::byte> payload);
    /// The game accepted the player: its slot is known, the shots of its weapon can be predicted.
    void accept(std::span<const std::byte> payload);
    /// Corrects the prediction with a part of a snapshot that has the ship of the player.
    void reconcile(const game::SnapshotPart &part);
    void sendInput();
    void sendVoiceState();
    [[nodiscard]] std::uint32_t estimateTick() const;
    /// Runs the voice of the game, once per update.
    void updateVoice();
    /// Moves the player to the voice of Discord once its call is connected, back to the voice of the game if
    /// it fails, and joins it again later if it dropped.
    void updateDiscord(voice::TalkClock::time_point now);
    /// The call of Discord dropped, or could not be joined: the voice of the game, and the call joined again
    /// later if it was connected once and retries are left; given up otherwise.
    void leaveDiscord(voice::TalkClock::time_point now);
    /// Where the GameVoice takes the voice of the player from, on Discord: its microphone, unless Discord
    /// gives none while the player talks.
    void updateDiscordCapture(bool talking, voice::TalkClock::time_point now);
    /// Another player of the game is on the voice of the game: on Discord, the player sends it VOICE too.
    [[nodiscard]] bool anyOnGame() const;
    /// Hears each other player as the player chose for it.
    void applyPlayerVoices(voice::VoiceChannel &channel);
    /// Tells the game what the player does with its voice, if that changed (VOICE_STATUS).
    void tellVoiceStatus();

    // The transport of the GameVoice
    bool send(const Outgoing &frame) override;
    [[nodiscard]] std::vector<Incoming> receive() override;

public:
    /// Starts connecting at once.
    /// @param game The address of the instance: the IP of the master, the UDP port of GAME_JOINED.
    /// @param config The definitions of the client (loadGameConfig()): the weapon of the player, whose shots
    ///   are predicted. Without it, they are drawn when the game sends them.
    /// @param voiceDevices The devices of the voice, which must outlive the session. Without them, the
    ///   session runs no voice.
    /// @param server The address of the server, as the player wrote it ("host:port"): what the choices for
    ///   the other players are kept under (playerVoiceKey()).
    /// @param playerId The id the master gave the player (WELCOME): its member of the Discord lobby has it.
    ///   0 joins no Discord lobby.
    /// @throws std::system_error if no UDP socket can be opened.
    RemoteSession(network::EventLoop &loop, const network::Address &game,
                  const lobby::GameJoinedMessage &joined, const game::GameConfig &config = {},
                  VoiceDevices *voiceDevices = nullptr, std::string server = {}, std::uint32_t playerId = 0);
    /// Says goodbye to the game (DISCONNECT) if still in it.
    ~RemoteSession() override;

    RemoteSession(const RemoteSession &) = delete;
    RemoteSession &operator=(const RemoteSession &) = delete;

    [[nodiscard]] std::uint8_t localSlot() const override { return _accepted ? _accepted->slot : 0; }
    void setInput(game::PlayerInput input) override { _input = input; }
    void update(float elapsed) override;
    [[nodiscard]] std::vector<game::Snapshot> takeSnapshots() override;
    /// Nothing: online, the game goes on.
    void setPaused(bool /*paused*/) override {}
    [[nodiscard]] bool isOver() const override { return _over; }
    [[nodiscard]] std::optional<std::chrono::milliseconds> ping() const override;
    [[nodiscard]] bool isEndless() const override { return true; }
    [[nodiscard]] bool isOnline() const override { return true; }
    [[nodiscard]] std::map<std::uint8_t, std::string> playerNames() const override { return _names; }
    [[nodiscard]] std::optional<VoiceView> voiceView() const override;

    /// What carries the voice of the player: VOICE_STATE, now if the game accepted the player, then at its
    /// acceptance. None, the default, hears and says nothing.
    void setVoiceBackend(voice::Backend backend);
    /// Sends a frame of the voice of the player (VOICE), unreliable, with the next datagram.
    /// @return false before the game accepted the player, after the end, or on None.
    bool sendVoice(const game::VoiceMessage &frame);
    /// The frames of the voice of the others received since the last call, in the order they came.
    [[nodiscard]] std::vector<game::VoiceFrameMessage> takeVoiceFrames();
    /// What carries the voice of each player, from the latest VOICE_ROSTER.
    [[nodiscard]] const game::VoiceRosterMessage &voiceRoster() const { return _voiceRoster; }
    /// What each player tells about its voice, from the latest VOICE_STATUSES.
    [[nodiscard]] const game::VoiceStatusesMessage &voiceStatuses() const { return _voiceStatuses; }
    /// The game told that an administrator muted the voice of the player (VOICE_STATUSES).
    [[nodiscard]] bool mutedByAdmin() const;
    /// The voice of the game, given VoiceDevices.
    [[nodiscard]] voice::GameVoice *voice() const { return _voice.get(); }
    /// The voice of Discord, until it is given up.
    [[nodiscard]] discord::DiscordVoice *discordVoice() const { return _discord.get(); }
    /// The voice of the player goes through Discord.
    [[nodiscard]] bool onDiscord() const { return _onDiscord; }
    /// On Discord, the GameVoice sends what Discord captures, not the microphone of VoiceDevices.
    [[nodiscard]] bool capturesThroughDiscord() const { return _onDiscord && _discordCapture; }
    /// Where each other player is heard from, given VoiceDevices.
    [[nodiscard]] const voice::SharedPlayback *playbacks() const
    {
        return _playbacks ? &*_playbacks : nullptr;
    }
    /// The devices of the voice given, if any.
    [[nodiscard]] VoiceDevices *voiceDevices() const { return _devices; }
    /// What the choices for the player in a slot are kept under (playerVoiceKey()); none for a slot without a
    /// player.
    [[nodiscard]] std::optional<std::string> voiceKey(std::uint8_t slot) const;
    /// The account of the player in a slot, 0 for a guest; none for a slot without a player.
    [[nodiscard]] std::optional<std::uint64_t> accountOf(std::uint8_t slot) const;

    /// The game accepted the player.
    [[nodiscard]] bool accepted() const { return _accepted.has_value(); }
    /// The address of the server, as the player wrote it ("host:port"), and the id of the game on it: what a
    /// friend invited from Discord joins.
    [[nodiscard]] const std::string &server() const { return _server; }
    [[nodiscard]] std::uint32_t gameId() const { return _gameId; }
    /// Why the game ended, for the player.
    [[nodiscard]] const std::string &endReason() const { return _endReason; }
    /// The names of the players, by slot.
    [[nodiscard]] const std::map<std::uint8_t, std::string> &names() const { return _names; }
    /// The world as received, the newest, for the tests.
    [[nodiscard]] const game::RemoteWorld &world() const { return _world; }
    /// The world as drawn, for the tests.
    [[nodiscard]] const Interpolation &interpolation() const { return _interpolation; }
    /// The prediction of the ship of the player, for the tests.
    [[nodiscard]] const Prediction &prediction() const { return _prediction; }
    /// The shots of the player as drawn, for the tests.
    [[nodiscard]] const ShotPrediction &shots() const { return _shots; }
    /// The tick stamped on the newest input sent.
    [[nodiscard]] std::uint32_t inputTick() const { return _inputTick; }
    /// The pace of the inputs, for the tests.
    [[nodiscard]] const InputClock &inputClock() const { return _inputClock; }
};

} // namespace rtype::client
