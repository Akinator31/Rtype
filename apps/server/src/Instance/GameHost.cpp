#include "Instance/GameHost.hpp"

#include "rtype/game/Messages.hpp"
#include "rtype/log/Logger.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <utility>

namespace rtype::server
{

namespace
{

using rtype::network::Delivery;
using rtype::network::DisconnectReason;
using rtype::network::SessionEnd;
using Clock = GameHost::Clock;
const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("instance");
    return instance;
}

/// When a mute of `seconds` from `now` ends.
std::optional<Clock::time_point> mutedUntil(Clock::time_point now, std::uint32_t seconds)
{
    if (seconds == 0) {
        return std::nullopt;
    }
    if (seconds == VoiceMutedForTheGame) {
        return Clock::time_point::max();
    }
    return now + std::chrono::seconds(seconds);
}

/// The input lead of a SNAPSHOT: within what an i8 holds, the lowest value meaning none.
std::int8_t toInputLead(std::optional<std::int64_t> lead)
{
    if (!lead) {
        return rtype::game::NoInputLead;
    }
    return static_cast<std::int8_t>(std::clamp<std::int64_t>(*lead, rtype::game::NoInputLead + 1, 127));
}

} // namespace

GameHost::GameHost(rtype::network::EventLoop &loop, std::unique_ptr<GameTransport> transport,
                   std::uint8_t maxPlayers, GameThread &game, Handlers handlers, VoiceRules voice)
    : _transport(std::move(transport)), _game(game), _maxPlayers(std::min(maxPlayers, MaxInstancePlayers)),
      _handlers(std::move(handlers)), _voice(voice),
      _sessions(
          [this](const rtype::network::Address &to, std::span<const std::byte> datagram) {
              _transport->send(to, datagram);
          },
          {
              .onConnected = [this](std::uint32_t session,
                                    const rtype::network::Address &from) { connected(session, from); },
              .onMessage = [this](std::uint32_t session, std::uint8_t type,
                                  std::span<const std::byte> payload) { received(session, type, payload); },
              .onClosed = [this](std::uint32_t session, SessionEnd end) { closed(session, end); },
          }),
      _updates(loop)
{
    _transport->start([this](const rtype::network::Address &from, std::span<const std::byte> datagram) {
        _sessions.receive(from, datagram, Clock::now());
    });
    // Sends, resends, acknowledgements, heartbeats and timeouts, at the pace of the ticks, but on its own:
    // the network never waits for the game
    _updates.startRepeating(GameThread::TickInterval, [this] {
        const Clock::time_point now = Clock::now();
        expireVoiceMutes(now);
        _sessions.update(now);
    });
}

std::optional<std::uint8_t> GameHost::freeSlot() const
{
    for (std::uint8_t slot = 0; slot < _maxPlayers; ++slot) {
        if (std::ranges::none_of(_players, [slot](const auto &entry) { return entry.second.slot == slot; })) {
            return slot;
        }
    }
    return std::nullopt;
}

bool GameHost::addPlayer(const AddPlayerMessage &player)
{
    const bool known = std::ranges::any_of(
        _players, [&](const auto &entry) { return entry.second.playerId == player.playerId; });
    const std::optional<std::uint8_t> slot = freeSlot();
    if (known || _players.contains(player.sessionId) || !slot) {
        return false;
    }
    _sessions.add(player.sessionId, rtype::network::DatagramKey(player.clientKey),
                  rtype::network::DatagramKey(player.serverKey), Clock::now());
    Player added;
    added.playerId = player.playerId;
    added.slot = *slot;
    added.name = player.name;
    added.accountId = player.accountId;
    added.voiceRate = VoiceRate(_voice.maxFramesPerSecond);
    added.voiceMutedUntil = mutedUntil(Clock::now(), player.voiceMuted);
    _players.emplace(player.sessionId, std::move(added));
    logger().debug("Player {} ({}) expected on session {}, slot {}", player.name, player.playerId,
                   player.sessionId, *slot);
    return true;
}

void GameHost::connected(std::uint32_t session, const rtype::network::Address &from)
{
    const auto found = _players.find(session);
    if (found == _players.end()) {
        return;
    }
    Player &player = found->second;
    player.connected = true;
    _game.addPlayer(player.slot);
    logger().info("{} ({}) connected from {}, slot {}", player.name, player.playerId, from, player.slot);

    _sessions.send(session, rtype::game::message_type::Accepted,
                   rtype::game::encode(rtype::game::AcceptedMessage{.slot = player.slot,
                                                                    .tickRate = rtype::game::TickRate,
                                                                    .tick = _game.tick(),
                                                                    .voice = _voice.enabled}),
                   Delivery::Reliable);
    // The players already there, for the newcomer
    for (const auto &[otherSession, other] : _players) {
        if (other.connected && otherSession != session) {
            _sessions.send(session, rtype::game::message_type::PlayerJoined,
                           rtype::game::encode(rtype::game::PlayerJoinedMessage{other.slot, other.playerId,
                                                                                other.name, other.accountId}),
                           Delivery::Reliable);
        }
    }
    // The newcomer, for everyone, itself included
    sendToConnected(rtype::game::message_type::PlayerJoined,
                    rtype::game::encode(rtype::game::PlayerJoinedMessage{player.slot, player.playerId,
                                                                         player.name, player.accountId}),
                    Delivery::Reliable);
    // What carries the voice of the others, for the newcomer; its own is None until it says
    _sessions.send(session, rtype::game::message_type::VoiceRoster, rtype::game::encode(voiceRoster()),
                   Delivery::Reliable);
    if (player.voiceMuted(Clock::now())) {
        // Muted already by its account: the others see it at once
        sendVoiceStatuses();
    } else {
        _sessions.send(session, rtype::game::message_type::VoiceStatuses,
                       rtype::game::encode(voiceStatuses()), Delivery::Reliable);
    }
    if (_handlers.onConnected) {
        _handlers.onConnected(player.playerId);
    }
}

void GameHost::received(std::uint32_t session, std::uint8_t type, std::span<const std::byte> payload)
{
    const auto found = _players.find(session);
    if (found == _players.end()) {
        return;
    }
    switch (type) {
    case rtype::game::message_type::Input:
        receivedInput(found->second, session, payload);
        return;
    case rtype::game::message_type::Voice:
        receivedVoice(found->second, session, payload);
        return;
    case rtype::game::message_type::VoiceState:
        receivedVoiceState(found->second, session, payload);
        return;
    case rtype::game::message_type::VoiceStatus:
        receivedVoiceStatus(found->second, session, payload);
        return;
    default:
        logger().debug("Message {:#04x} of session {} ignored: not one a client sends", type, session);
        return;
    }
}

void GameHost::receivedInput(Player &player, std::uint32_t session, std::span<const std::byte> payload)
{
    std::optional<rtype::game::InputMessage> input = rtype::game::decodeInput(payload);
    if (!input) {
        logger().debug("Malformed INPUT of session {} dropped", session);
        return;
    }
    // Each one applied at its tick: the older ones are those a lost datagram did not bring
    _game.receiveInputs(player.slot, input->tick, std::move(input->inputs));
}

void GameHost::receivedVoice(Player &player, std::uint32_t session, std::span<const std::byte> payload)
{
    std::optional<rtype::game::VoiceMessage> voice = rtype::game::decodeVoice(payload);
    if (!voice) {
        logger().debug("Malformed VOICE of session {} dropped", session);
        return;
    }
    // The voice off, a player on None or muted talks to nobody; a frame bigger than the server allows, or
    // past its rate, is more than a voice
    const Clock::time_point now = Clock::now();
    if (!_voice.enabled || player.voice == rtype::voice::Backend::None || player.voiceMuted(now) ||
        voice->frame.size() > _voice.maxFrameBytes || !player.voiceRate.allow(now)) {
        ++player.voiceDropped;
        return;
    }
    const std::vector<std::byte> frame =
        rtype::game::encode(rtype::game::VoiceFrameMessage{.slot = player.slot,
                                                           .sequence = voice->sequence,
                                                           .last = voice->last,
                                                           .frame = std::move(voice->frame)});
    for (const auto &[otherSession, other] : _players) {
        if (other.connected && otherSession != session &&
            rtype::voice::carriedByGame(player.voice, other.voice)) {
            _sessions.send(otherSession, rtype::game::message_type::VoiceFrame, frame, Delivery::Unreliable);
        }
    }
}

void GameHost::receivedVoiceState(Player &player, std::uint32_t session, std::span<const std::byte> payload)
{
    const std::optional<rtype::game::VoiceStateMessage> state = rtype::game::decodeVoiceState(payload);
    if (!state) {
        logger().debug("Malformed VOICE_STATE of session {} dropped", session);
        return;
    }
    if (state->backend == player.voice) {
        return;
    }
    player.voice = state->backend;
    logger().debug("{} ({}) talks on the voice of {}", player.name, player.playerId,
                   rtype::voice::name(player.voice));
    sendVoiceRoster();
}

void GameHost::receivedVoiceStatus(Player &player, std::uint32_t session, std::span<const std::byte> payload)
{
    const std::optional<rtype::game::VoiceStatusMessage> status = rtype::game::decodeVoiceStatus(payload);
    if (!status) {
        logger().debug("Malformed VOICE_STATUS of session {} dropped", session);
        return;
    }
    if (status->status == player.voiceStatus) {
        return;
    }
    player.voiceStatus = status->status;
    sendVoiceStatuses();
}

rtype::game::VoiceStatusesMessage GameHost::voiceStatuses() const
{
    rtype::game::VoiceStatusesMessage statuses;
    const Clock::time_point now = Clock::now();
    for (const auto &[session, player] : _players) {
        if (player.connected && player.slot < statuses.statuses.size()) {
            statuses.statuses[player.slot] = player.voiceStatus;
            // The game's to say, whatever the player told
            statuses.statuses[player.slot].mutedByAdmin = player.voiceMuted(now);
        }
    }
    return statuses;
}

bool GameHost::muteVoice(std::uint32_t playerId, std::uint32_t seconds)
{
    const auto found = std::ranges::find_if(
        _players, [playerId](const auto &entry) { return entry.second.playerId == playerId; });
    if (found == _players.end()) {
        return false;
    }
    Player &player = found->second;
    const Clock::time_point now = Clock::now();
    const bool before = player.voiceMuted(now);
    player.voiceMutedUntil = mutedUntil(now, seconds);
    if (seconds == 0) {
        logger().info("{} ({}) may talk again", player.name, playerId);
    } else if (seconds == VoiceMutedForTheGame) {
        logger().info("{} ({}) muted by an administrator for the rest of the game", player.name, playerId);
    } else {
        logger().info("{} ({}) muted by an administrator for {} s", player.name, playerId, seconds);
    }
    if (player.connected && before != player.voiceMuted(now)) {
        sendVoiceStatuses();
    }
    return true;
}

void GameHost::expireVoiceMutes(Clock::time_point now)
{
    bool expired = false;
    for (auto &[session, player] : _players) {
        if (player.voiceMutedUntil && now >= *player.voiceMutedUntil) {
            player.voiceMutedUntil.reset();
            logger().info("{} ({}) may talk again: its mute ended", player.name, player.playerId);
            expired = expired || player.connected;
        }
    }
    if (expired) {
        sendVoiceStatuses();
    }
}

void GameHost::sendVoiceStatuses()
{
    sendToConnected(rtype::game::message_type::VoiceStatuses, rtype::game::encode(voiceStatuses()),
                    Delivery::Reliable);
}

rtype::game::VoiceRosterMessage GameHost::voiceRoster() const
{
    rtype::game::VoiceRosterMessage roster;
    for (const auto &[session, player] : _players) {
        if (player.connected && player.slot < roster.backends.size()) {
            roster.backends[player.slot] = player.voice;
        }
    }
    return roster;
}

void GameHost::sendVoiceRoster()
{
    sendToConnected(rtype::game::message_type::VoiceRoster, rtype::game::encode(voiceRoster()),
                    Delivery::Reliable);
}

void GameHost::leave(std::uint32_t session, rtype::game::LeaveReason reason)
{
    const auto found = _players.find(session);
    if (found == _players.end()) {
        return;
    }
    const Player player = found->second;
    _players.erase(found);
    if (player.voiceDropped > 0) {
        logger().debug("{} ({}): {} frames of voice dropped", player.name, player.playerId,
                       player.voiceDropped);
    }
    if (player.connected) {
        _game.removePlayer(player.slot);
        sendToConnected(rtype::game::message_type::PlayerLeft,
                        rtype::game::encode(rtype::game::PlayerLeftMessage{player.slot, reason}),
                        Delivery::Reliable);
        if (player.voice != rtype::voice::Backend::None) {
            sendVoiceRoster();
        }
        if (player.voiceStatus != rtype::voice::Status{}) {
            sendVoiceStatuses();
        }
    }
}

void GameHost::closed(std::uint32_t session, SessionEnd end)
{
    const auto found = _players.find(session);
    if (found == _players.end()) {
        return;
    }
    const Player player = found->second;
    const bool quit = end.cause == SessionEnd::Cause::Disconnected;
    if (player.connected) {
        logger().info("{} ({}) {}", player.name, player.playerId, quit ? "left" : "lost the connection");
    } else {
        logger().info("{} ({}) never connected", player.name, player.playerId);
    }
    leave(session, quit ? rtype::game::LeaveReason::Quit : rtype::game::LeaveReason::TimedOut);
    if (_handlers.onDisconnected) {
        _handlers.onDisconnected(player.playerId,
                                 quit ? PlayerDisconnectedReason::Quit : PlayerDisconnectedReason::TimedOut);
    }
}

bool GameHost::removePlayer(std::uint32_t playerId, RemovePlayerReason reason)
{
    const auto found = std::ranges::find_if(
        _players, [playerId](const auto &entry) { return entry.second.playerId == playerId; });
    if (found == _players.end()) {
        return false;
    }
    const std::uint32_t session = found->first;
    logger().info("{} ({}) removed by {}", found->second.name, playerId,
                  reason == RemovePlayerReason::Kicked ? "an administrator" : "the master");
    _sessions.close(session, DisconnectReason::Quit);
    leave(session, reason == RemovePlayerReason::LobbyClosed ? rtype::game::LeaveReason::LobbyClosed
                                                             : rtype::game::LeaveReason::Quit);
    return true;
}

void GameHost::sendToConnected(std::uint8_t type, std::span<const std::byte> payload, Delivery delivery,
                               std::optional<std::uint32_t> except)
{
    for (const auto &[session, player] : _players) {
        if (player.connected && session != except) {
            _sessions.send(session, type, payload, delivery);
        }
    }
}

void GameHost::publish(const TickResult &result)
{
    if (_over) {
        return;
    }
    const rtype::game::Snapshot &snapshot = result.snapshot;
    _tick = snapshot.tick;
    _wave = snapshot.waveStarted.value_or(_wave);
    _states = snapshot.players;
    for (std::size_t slot = 0; slot < _inputLeads.size(); ++slot) {
        if (const std::optional<std::int64_t> lead = result.inputLeads[slot]) {
            _inputLeads[slot] = _inputLeads[slot] ? std::min(*_inputLeads[slot], *lead) : *lead;
        }
    }
    // Unreliable, as soon as they happen, then again with the snapshots: a lost one costs a snapshot
    // interval, and holds back nothing behind it
    const std::vector<rtype::game::EntitiesDestroyedMessage> destroyed =
        rtype::game::destroyedMessages(snapshot);
    for (const rtype::game::EntitiesDestroyedMessage &message : destroyed) {
        sendToConnected(rtype::game::message_type::EntitiesDestroyed, rtype::game::encode(message),
                        Delivery::Unreliable);
    }
    _destructions.add(snapshot.tick, destroyed);
    if (snapshot.waveStarted) {
        sendToConnected(
            rtype::game::message_type::WaveStarted,
            rtype::game::encode(rtype::game::WaveStartedMessage{*snapshot.waveStarted, snapshot.waveMusic}),
            Delivery::Reliable);
    }
    // The last snapshot goes with the game over, whatever its tick: the final scores
    if (snapshot.tick % SnapshotInterval == 0 || snapshot.gameOver) {
        for (const auto &[session, player] : _players) {
            if (!player.connected) {
                continue;
            }
            for (const rtype::game::SnapshotPart &part : rtype::game::splitSnapshot(
                     snapshot, result.lastInputs[player.slot], toInputLead(_inputLeads[player.slot]),
                     result.weapons[player.slot])) {
                _sessions.send(session, rtype::game::message_type::Snapshot, rtype::game::encode(part),
                               Delivery::Unreliable);
            }
        }
        _inputLeads.fill(std::nullopt);
        for (const rtype::game::EntitiesDestroyedMessage &message : _destructions.again(snapshot.tick)) {
            sendToConnected(rtype::game::message_type::EntitiesDestroyed, rtype::game::encode(message),
                            Delivery::Unreliable);
        }
    }
    if (snapshot.gameOver) {
        gameOver(result);
    }
    // Sent now: waiting for the next update of the sessions would add up to a tick of delay, never the same
    _sessions.update(Clock::now());
}

void GameHost::gameOver(const TickResult &result)
{
    _over = true;
    const rtype::game::GameOutcome outcome = *result.snapshot.gameOver;
    logger().info("Game over: {}", outcome == rtype::game::GameOutcome::Victory ? "victory" : "defeat");
    sendToConnected(rtype::game::message_type::GameOver,
                    rtype::game::encode(rtype::game::GameOverMessage{outcome}), Delivery::Reliable);

    GameOverReport report;
    for (const auto &[session, player] : _players) {
        const auto state =
            std::ranges::find(result.snapshot.players, player.slot, &rtype::game::PlayerState::slot);
        if (player.connected && state != result.snapshot.players.end()) {
            report.scores.push_back({.playerId = player.playerId,
                                     .score = state->score,
                                     .kills = state->kills,
                                     .wavesCleared = state->wavesCleared,
                                     .bestStreak = state->bestStreak,
                                     .xp = state->xp});
        }
    }
    if (_handlers.onGameOver) {
        _handlers.onGameOver(report);
    }
}

GameStatusMessage GameHost::status() const
{
    GameStatusMessage status{.tick = _tick, .wave = _wave, .players = {}};
    for (const auto &[session, player] : _players) {
        if (!player.connected) {
            continue;
        }
        const auto state = std::ranges::find(_states, player.slot, &rtype::game::PlayerState::slot);
        PlayerStatus entry{.playerId = player.playerId,
                           .score = state == _states.end() ? 0 : state->score,
                           .alive = state != _states.end() && state->lives > 0,
                           .latency = std::nullopt};
        if (const std::optional<Clock::duration> rtt = _sessions.rtt(session)) {
            const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(*rtt).count();
            entry.latency = static_cast<std::uint16_t>(
                std::clamp<std::chrono::milliseconds::rep>(milliseconds, 0, MaxStatusLatency));
        }
        status.players.push_back(entry);
    }
    return status;
}

bool GameHost::acknowledged() const
{
    return std::ranges::all_of(_players, [this](const auto &entry) {
        return !entry.second.connected || _sessions.pendingReliable(entry.first) == 0;
    });
}

void GameHost::closeAll(DisconnectReason reason)
{
    _updates.cancel();
    _sessions.closeAll(reason);
    _players.clear();
    _transport->close();
}

} // namespace rtype::server
