#pragma once

#include "Game/GameSession.hpp"
#include "Game/InputClock.hpp"
#include "Game/Interpolation.hpp"
#include "Game/Prediction.hpp"
#include "Game/ShotPrediction.hpp"
#include "Voice/VoiceDevices.hpp"
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

/// Gives the voice of the game how the player talks: its mode (push-to-talk, voice activity or off), muted
/// and deafened by its keys, and its volumes.
/// @param pushToTalk The push-to-talk key is held, or was released less than voice::PushToTalk::ReleaseDelay
///   ago.
void applyVoice(voice::GameVoice &voice, const VoiceDevices &devices, bool pushToTalk);

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
///   in the pause menu (VoicePreferences::players, under voiceKey()).
/// - **The end**: DISCONNECT from the game, a timeout, or GAME_OVER; endReason() tells the player why.
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
    std::unique_ptr<voice::GameVoice> _voice;

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
    /// Hears each other player as the player chose for it.
    void applyPlayerVoices();
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
    /// @throws std::system_error if no UDP socket can be opened.
    RemoteSession(network::EventLoop &loop, const network::Address &game,
                  const lobby::GameJoinedMessage &joined, const game::GameConfig &config = {},
                  VoiceDevices *voiceDevices = nullptr, std::string server = {});
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
    /// The voice of the game, given VoiceDevices.
    [[nodiscard]] voice::GameVoice *voice() const { return _voice.get(); }
    /// The devices of the voice given, if any.
    [[nodiscard]] VoiceDevices *voiceDevices() const { return _devices; }
    /// What the choices for the player in a slot are kept under (playerVoiceKey()); none for a slot without a
    /// player.
    [[nodiscard]] std::optional<std::string> voiceKey(std::uint8_t slot) const;
    /// The account of the player in a slot, 0 for a guest; none for a slot without a player.
    [[nodiscard]] std::optional<std::uint64_t> accountOf(std::uint8_t slot) const;

    /// The game accepted the player.
    [[nodiscard]] bool accepted() const { return _accepted.has_value(); }
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
