#pragma once

#include "Common/InstanceMessages.hpp"
#include "Instance/GameThread.hpp"
#include "Instance/GameTransport.hpp"
#include "Instance/RecentDestructions.hpp"
#include "Instance/VoiceRate.hpp"
#include "rtype/game/Messages.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/Session.hpp"
#include "rtype/network/Timer.hpp"
#include "rtype/voice/Backend.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rtype::server
{

/// The network side of a game, on the network thread of an instance: its transport, the sessions of its
/// players, their slots, and the messages of the game protocol (docs/protocol/game.md). It hands the
/// players and their inputs to the GameThread, and sends the result of each tick to the clients.
///
/// @code
/// rtype::server::GameHost host(loop,
///     std::make_unique<rtype::server::DirectTransport>(loop, rtype::network::Address::anyIpv4(udpPort)), 4,
///     game,
///     {.onConnected = [&](std::uint32_t player) { link.sendPlayerConnected(player); },
///      .onDisconnected = [&](std::uint32_t player, auto reason) { ... }});
/// host.addPlayer(addPlayer);       // ADD_PLAYER from the master
/// host.publish(result);            // each tick, from the GameThread through loop.post()
/// @endcode
///
/// - The datagrams come and go through the transport given (GameTransport.hpp), with the real address of each
///   client: the host does not know whether they come from a socket of its own or from a relay.
/// - A player gets the lowest free slot when added, and its ship when its client connects.
/// - On `CONNECT`, the client receives `ACCEPTED` (its slot, the tick rate, the tick), then `PLAYER_JOINED`
///   for each player connected, itself included, with the id of its account; the others receive its
///   `PLAYER_JOINED`.
/// - The inputs of each `INPUT` go to the game, which applies each one at its tick.
/// - The voice (section 9.4, ADR 0032): each player tells what carries its voice (`VOICE_STATE`, None until
///   then), and every client knows it for all (`VOICE_ROSTER`, at its connection and at every change). Each
///   player also tells whether it is muted, deafened or without a microphone (`VOICE_STATUS`), and every
///   client knows it for all in the same way (`VOICE_STATUSES`), for its HUD. Each
///   `VOICE` goes as `VOICE_FRAME` to the other players, unless both are on Discord
///   (rtype::voice::carriedByGame()), and within the rules of the server (VoiceRules: the voice on, the
///   largest frame, the rate of VoiceRate). A player muted by an administrator (`ADD_PLAYER`, `MUTE_VOICE`)
///   has its voice dropped until its mute ends, and the others see it in `VOICE_STATUSES`. The game never
///   decodes the voice.
/// - Every 2 ticks, each client receives the snapshot in parts; every tick, the destroyed entities. They
///   leave at once, not at the next update of the sessions: no delay, no jitter.
/// - A session that closes takes the ship away, and the others receive `PLAYER_LEFT`.
/// - The game over is sent to every client (`GAME_OVER`), then told to the instance with the scores; nothing
///   is published after it.
/// - status() tells where the game stands, from the last result published and the round-trip times of the
///   sessions (`GAME_STATUS` for the master). The game thread hands over its numbers with each result: it
///   holds no lock for them, and the network thread keeps only the last.
class GameHost
{
public:
    struct Handlers {
        /// The client of a player connected (`PLAYER_CONNECTED` for the master).
        std::function<void(std::uint32_t playerId)> onConnected;
        /// The session of a player closed on the side of the game, or never connected
        /// (`PLAYER_DISCONNECTED`). Not called for removePlayer() and closeAll().
        std::function<void(std::uint32_t playerId, PlayerDisconnectedReason reason)> onDisconnected;
        /// The game is over, with the score of each player (`GAME_OVER` for the master). Once.
        std::function<void(const GameOverReport &report)> onGameOver;
    };

    using Clock = rtype::network::SessionHost::Clock;

    /// Snapshots are sent once every this many ticks: 30 per second (docs/protocol/game.md, section 9.3).
    static constexpr std::uint32_t SnapshotInterval = 2;

private:
    struct Player {
        std::uint32_t playerId = 0;
        std::uint8_t slot = 0;
        std::string name;
        /// The id of its account, 0 for a guest.
        std::uint64_t accountId = 0;
        bool connected = false;
        /// What carries its voice (`VOICE_STATE`).
        rtype::voice::Backend voice = rtype::voice::Backend::None;
        /// What it tells about its voice (`VOICE_STATUS`).
        rtype::voice::Status voiceStatus{};
        VoiceRate voiceRate;
        /// Muted by an administrator until then: time_point::max() for the rest of the game.
        std::optional<Clock::time_point> voiceMutedUntil;
        /// Frames of its voice dropped: too many, too big, sent on None, muted, or the voice off. Logged
        /// when it leaves.
        std::uint32_t voiceDropped = 0;

        [[nodiscard]] bool voiceMuted(Clock::time_point now) const
        {
            return voiceMutedUntil && now < *voiceMutedUntil;
        }
    };

    std::unique_ptr<GameTransport> _transport;
    GameThread &_game;
    std::uint8_t _maxPlayers;
    Handlers _handlers;
    VoiceRules _voice;
    rtype::network::SessionHost _sessions;
    rtype::network::Timer _updates;
    /// By session id.
    std::map<std::uint32_t, Player> _players;
    bool _over = false;
    /// From the last result published, for status().
    std::uint32_t _tick = 0;
    std::uint16_t _wave = 0;
    std::vector<rtype::game::PlayerState> _states;
    /// For each slot, the smallest lead of its INPUT messages since the previous snapshot
    /// (TickResult::inputLeads).
    std::array<std::optional<std::int64_t>, rtype::game::MaxPlayers> _inputLeads{};
    /// The destructions of the last ticks, sent again with each snapshot.
    RecentDestructions _destructions;

    void gameOver(const TickResult &result);

    [[nodiscard]] std::optional<std::uint8_t> freeSlot() const;
    void connected(std::uint32_t session, const rtype::network::Address &from);
    void received(std::uint32_t session, std::uint8_t type, std::span<const std::byte> payload);
    /// `INPUT`, `VOICE`, `VOICE_STATE` and `VOICE_STATUS` of a player.
    void receivedInput(Player &player, std::uint32_t session, std::span<const std::byte> payload);
    void receivedVoice(Player &player, std::uint32_t session, std::span<const std::byte> payload);
    void receivedVoiceState(Player &player, std::uint32_t session, std::span<const std::byte> payload);
    void receivedVoiceStatus(Player &player, std::uint32_t session, std::span<const std::byte> payload);
    /// What carries the voice of each connected player, by slot.
    [[nodiscard]] rtype::game::VoiceRosterMessage voiceRoster() const;
    /// `VOICE_ROSTER` to every connected player.
    void sendVoiceRoster();
    /// What each connected player tells about its voice, by slot.
    [[nodiscard]] rtype::game::VoiceStatusesMessage voiceStatuses() const;
    /// `VOICE_STATUSES` to every connected player.
    void sendVoiceStatuses();
    /// Lets talk again the players whose mute ended, and tells the others.
    void expireVoiceMutes(Clock::time_point now);
    void closed(std::uint32_t session, rtype::network::SessionEnd end);
    /// The player leaves: its ship goes, the others are told.
    void leave(std::uint32_t session, rtype::game::LeaveReason reason);
    void sendToConnected(std::uint8_t type, std::span<const std::byte> payload,
                         rtype::network::Delivery delivery,
                         std::optional<std::uint32_t> except = std::nullopt);

public:
    /// Starts receiving through the transport, already open: whatever cannot be opened has failed before.
    /// @param voice What the game does with the voice (CONFIGURE).
    GameHost(rtype::network::EventLoop &loop, std::unique_ptr<GameTransport> transport,
             std::uint8_t maxPlayers, GameThread &game, Handlers handlers, VoiceRules voice = {});

    GameHost(const GameHost &) = delete;
    GameHost &operator=(const GameHost &) = delete;

    [[nodiscard]] const rtype::network::Address &localAddress() const { return _transport->localAddress(); }

    /// Expects the client of a player on its session.
    /// @return false if the game is full, or the player or its session already exists.
    bool addPlayer(const AddPlayerMessage &player);

    /// Closes the session of a player (`DISCONNECT (QUIT)`), and tells the others.
    /// @return false if the player is unknown: its session had already closed.
    bool removePlayer(std::uint32_t playerId, RemovePlayerReason reason);

    /// Mutes the voice of a player for `seconds` from now (`MUTE_VOICE`): 0 lets it talk again,
    /// VoiceMutedForTheGame until the end of the game.
    /// @return false if the player is unknown.
    bool muteVoice(std::uint32_t playerId, std::uint32_t seconds);
    /// Sends the result of a tick to the clients. On the network thread.
    void publish(const TickResult &result);

    /// Closes every session with `DISCONNECT` and this reason, then the transport, after the datagrams
    /// queued: the game stops.
    void closeAll(rtype::network::DisconnectReason reason);

    /// Where the game stands: the last tick published, the wave under way, and each player whose client
    /// connected, with its score, its lives and its round-trip time.
    [[nodiscard]] GameStatusMessage status() const;

    /// Number of players added, connected or not.
    [[nodiscard]] std::size_t playerCount() const { return _players.size(); }

    /// Every client acknowledged every reliable message sent to it: `GAME_OVER`, at the end of a game.
    [[nodiscard]] bool acknowledged() const;
    /// The game is over.
    [[nodiscard]] bool over() const { return _over; }
};

} // namespace rtype::server
