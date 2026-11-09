#include "Game/RemoteSession.hpp"

#include "rtype/game/Constants.hpp"
#include "rtype/game/Hitbox.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/Datagram.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

namespace rtype::client
{

namespace
{

constexpr double Tick = 1.0 / game::TickRate;
/// Most inputs sent in one update, after a long frame.
constexpr int MaxInputsPerUpdate = 5;
/// The round-trip time assumed before one is measured.
constexpr std::chrono::milliseconds DefaultRtt{100};
/// Ticks of margin, so that an input arrives before its tick (docs/protocol/game.md, 9.2).
constexpr std::uint32_t InputMargin = 2;
/// The time between two snapshots, in seconds: what a jump of the stamps takes to show, on top of the RTT.
constexpr double SnapshotDelay = 2.0 / game::TickRate;

const log::Logger &logger()
{
    static const log::Logger instance("game");
    return instance;
}

/// The weapon the players start with, if the definitions have it.
std::optional<game::WeaponDefinition> playerWeapon(const game::GameConfig &config)
{
    const auto weapon = std::ranges::find(config.weapons, config.player.weapon, &game::WeaponDefinition::id);
    if (config.player.weapon.empty() || weapon == config.weapons.end()) {
        return std::nullopt;
    }
    if (std::ranges::find(config.projectiles, weapon->projectile, &game::ProjectileDefinition::id) ==
        config.projectiles.end()) {
        return std::nullopt;
    }
    return *weapon;
}

/// Half the width of the projectile of a weapon, where its shots start.
float projectileHalfWidth(const game::GameConfig &config, const game::WeaponDefinition &weapon)
{
    const auto projectile =
        std::ranges::find(config.projectiles, weapon.projectile, &game::ProjectileDefinition::id);
    return game::halfSizeOf(projectile->collider).x;
}

std::string reasonOf(const network::SessionEnd &end)
{
    switch (end.cause) {
    case network::SessionEnd::Cause::Disconnected:
        switch (end.reason) {
        case network::DisconnectReason::GameOver:
            return "The game is over";
        case network::DisconnectReason::Shutdown:
            return "The game stopped";
        case network::DisconnectReason::ProtocolError:
            return "The game and the client disagree";
        case network::DisconnectReason::Quit:
            return "Left the game";
        }
        return "Left the game";
    case network::SessionEnd::Cause::TimedOut:
        return "Connection to the game lost";
    case network::SessionEnd::Cause::ConnectTimedOut:
        return "The game cannot be reached: its UDP port may be blocked";
    case network::SessionEnd::Cause::Failed:
        return "The game stopped answering";
    }
    return "The game ended";
}

} // namespace

RemoteSession::RemoteSession(network::EventLoop &loop, const network::Address &game,
                             const lobby::GameJoinedMessage &joined, const game::GameConfig &config)
    : _socket(loop, game.isIpv6() ? network::Address::anyIpv6(0) : network::Address::anyIpv4(0)), _game(game),
      _session(joined.sessionId, network::DatagramKey(joined.clientKey),
               network::DatagramKey(joined.serverKey),
               [this](std::span<const std::byte> datagram) { _socket.send(_game, datagram); })
{
    _weapon = playerWeapon(config);
    if (_weapon) {
        _projectileHalfWidth = projectileHalfWidth(config, *_weapon);
    }
    _socket.startReceiving([this](const network::Address &from, std::span<const std::byte> datagram) {
        if (from == _game) {
            _session.receive(datagram, Clock::now());
        }
    });
    logger().info("Joining game {} on {}", joined.gameId, game);
    _session.start([this](std::span<const std::byte> payload) { accept(payload); },
                   [this](std::uint8_t type, std::span<const std::byte> payload) { received(type, payload); },
                   [this](network::SessionEnd end) {
                       if (!_over) {
                           _over = true;
                           _endReason = reasonOf(end);
                       }
                       logger().info("{}", _endReason);
                   },
                   Clock::now());
}

void RemoteSession::accept(std::span<const std::byte> payload)
{
    _accepted = game::decodeAccepted(payload);
    if (!_accepted) {
        _over = true;
        _endReason = "The game and the client disagree";
        return;
    }
    _snapshotReceived = Clock::now();
    logger().info("In the game, slot {}", _accepted->slot);
    if (_weapon) {
        _prediction.arm(*_weapon, _projectileHalfWidth, _accepted->slot);
        _shots.arm(_weapon->mechanic, game::Rules{});
    }
}

RemoteSession::~RemoteSession()
{
    if (_session.state() == network::ClientSession::State::Connected ||
        _session.state() == network::ClientSession::State::Connecting) {
        _session.close(network::DisconnectReason::Quit);
    }
}

void RemoteSession::received(std::uint8_t type, std::span<const std::byte> payload)
{
    switch (type) {
    case game::message_type::Snapshot:
        if (const auto part = game::decodeSnapshotPart(payload); part && _world.apply(*part)) {
            // Every copy of their destruction lost: gone all the same
            (void)_world.forgetStale();
            _interpolation.receive(_world.tick(), _world.entities(), _world.players());
            _snapshotReceived = Clock::now();
            reconcile(*part);
            if (_accepted) {
                // Each shot of the player takes the place of the one predicted at the tick it was fired at
                for (const game::EntityState &entity : part->entities) {
                    if (ShotPrediction::isShotOf(entity, _accepted->slot)) {
                        _shots.received(entity, part->tick);
                    }
                }
            }
            if (part->inputLead != game::NoInputLead && part->tick > _leadTick) {
                // Once per snapshot: every part has the same
                _leadTick = part->tick;
                const double rtt = std::chrono::duration<double>(_session.rtt().value_or(DefaultRtt)).count();
                _inputClock.receive(part->inputLead, rtt + SnapshotDelay);
            }
        }
        return;
    case game::message_type::EntitiesDestroyed:
        if (const auto destroyed = game::decodeEntitiesDestroyed(payload)) {
            // Drawn until the drawn tick reaches its destruction, 100 ms after the message arrives
            for (const game::DestroyedEntity &entity : destroyed->entities) {
                _interpolation.destroyed(entity.id, destroyed->tick);
                if (const game::EntityState *known = _world.find(entity.id);
                    known != nullptr && _accepted && ShotPrediction::isShotOf(*known, _accepted->slot)) {
                    _shots.destroyed(entity.id, destroyed->tick);
                }
                if (entity.id == _shipId) {
                    // The ship of the player is no more: nothing to predict until it comes back
                    _prediction.reconcile(std::nullopt, destroyed->tick);
                    _shipId = 0;
                }
            }
            for (const game::RemovedEntity &removed : _world.apply(*destroyed)) {
                _destroyed.push_back({removed.last.id, removed.cause});
            }
        }
        return;
    case game::message_type::PlayerJoined:
        if (const auto joined = game::decodePlayerJoined(payload)) {
            _names[joined->slot] = joined->name;
        }
        return;
    case game::message_type::PlayerLeft:
        if (const auto left = game::decodePlayerLeft(payload)) {
            _names.erase(left->slot);
        }
        return;
    case game::message_type::GameOver:
        if (const auto over = game::decodeGameOver(payload)) {
            _over = true;
            _outcome = over->outcome;
            _endReason = over->outcome == game::GameOutcome::Victory ? "Victory!" : "Game over";
        }
        return;
    case game::message_type::WaveStarted:
        if (const auto started = game::decodeWaveStarted(payload)) {
            _waveStarted = started->wave;
            _waveMusic = started->music;
        }
        return;
    default:
        // The kinds of messages to come
        return;
    }
}

void RemoteSession::reconcile(const game::SnapshotPart &part)
{
    if (!_accepted) {
        return;
    }
    for (const game::EntityState &entity : part.entities) {
        if (entity.kind == game::EntityKind::PlayerShip && entity.owner == _accepted->slot) {
            _shipId = entity.id;
            _prediction.reconcile(ecs::Position{entity.x, entity.y}, part.tick, part.weapon);
            return;
        }
    }
    // Missing from a snapshot in one part: the ship is not there. From a part among others, it may be in
    // another one
    if (part.parts == 1) {
        _prediction.reconcile(std::nullopt, part.tick);
        _shipId = 0;
    }
}

std::uint32_t RemoteSession::estimateTick() const
{
    // The tick of the latest snapshot, plus the time since it came, plus the way back to the game
    const std::uint32_t base = _world.hasSnapshot() ? _world.tick() : (_accepted ? _accepted->tick : 0);
    const double since = std::chrono::duration<double>(Clock::now() - _snapshotReceived).count();
    const double rtt = std::chrono::duration<double>(_session.rtt().value_or(DefaultRtt)).count();
    return base + static_cast<std::uint32_t>(std::lround((since + rtt) * game::TickRate)) + InputMargin;
}

void RemoteSession::sendInput()
{
    std::uint32_t stamp = _inputTick + 1;
    if (!_inputClock.synced()) {
        // Never back in time: the game ignores an input older than the last one
        stamp = std::max(stamp, estimateTick());
    }
    if (const std::uint32_t jump = _inputClock.takeJump(); jump > 0) {
        logger().debug("Inputs too late for the game: stamped {} ticks further", jump);
        stamp += jump;
    }
    // The ticks skipped repeat the previous input, as the game and the prediction do
    for (std::uint32_t skipped = _inputTick + 1;
         skipped < stamp && !_history.empty() && skipped - _inputTick <= InputHistory; ++skipped) {
        _history.push_front(_history.front());
    }
    _inputTick = stamp;
    // The ship of the player moves at once, as the game will move it at that tick
    _prediction.apply(_inputTick, _input);
    _history.push_front(_input);
    while (_history.size() > InputHistory || _history.size() > std::size_t{_inputTick} + 1) {
        _history.pop_back();
    }
    const game::InputMessage message{_inputTick, {_history.begin(), _history.end()}};
    _session.send(game::message_type::Input, game::encode(message), network::Delivery::Unreliable);
}

void RemoteSession::update(float elapsed)
{
    _session.update(Clock::now());
    _interpolation.advance(elapsed);
    _prediction.advance(elapsed);
    _inputClock.advance(elapsed);
    if (!_accepted || _over) {
        return;
    }
    // One INPUT per tick, whatever the frame rate, a little faster or slower to arrive just in time
    _pending += std::max(0.0, static_cast<double>(elapsed) * _inputClock.rate());
    int sent = 0;
    while (_pending >= Tick && sent < MaxInputsPerUpdate) {
        sendInput();
        _pending -= Tick;
        ++sent;
    }
    if (sent == MaxInputsPerUpdate) {
        _pending = std::min(_pending, Tick);
    }
}

std::optional<std::chrono::milliseconds> RemoteSession::ping() const
{
    const std::optional<Clock::duration> rtt = _session.rtt();
    if (!rtt) {
        return std::nullopt;
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(*rtt);
}

std::vector<game::Snapshot> RemoteSession::takeSnapshots()
{
    if (!_interpolation.drawnTick()) {
        return {};
    }
    // Each frame: the world drawn moves at the frame rate
    game::Snapshot snapshot = _interpolation.view();
    // The ship of the player where it is predicted, aiming as its input says, the rest where the
    // interpolation draws it
    // Between two inputs sent, at the frame rate: the fraction of the time to the next one
    const double fraction = std::clamp(_pending / Tick, 0.0, 1.0);
    const std::optional<ecs::Position> predicted = _prediction.position(fraction, _input);
    for (game::EntityState &entity : snapshot.entities) {
        if (entity.kind != game::EntityKind::PlayerShip || entity.owner != localSlot()) {
            continue;
        }
        if (predicted) {
            entity.x = predicted->x;
            entity.y = predicted->y;
        }
        const int aim = _input.aim();
        entity.flags &=
            static_cast<std::uint8_t>(~(game::EntityState::AimingUp | game::EntityState::AimingDown));
        if (aim < 0) {
            entity.flags |= game::EntityState::AimingUp;
        } else if (aim > 0) {
            entity.flags |= game::EntityState::AimingDown;
        }
    }
    // The shots of the player from the moment it fires, in the time of its ship, not in the past with the
    // rest
    _shots.predict(_prediction.shots(), _world.tick());
    _shots.apply(snapshot, localSlot(), *_interpolation.drawnTick(),
                 static_cast<double>(_inputTick) + fraction);
    snapshot.destroyed = std::exchange(_destroyed, {});
    for (game::DestroyedEntity &entity : snapshot.destroyed) {
        // Drawn under the id of the shot predicted, if it took its place
        entity.id = _shots.idOf(entity.id);
    }
    snapshot.waveStarted = std::exchange(_waveStarted, std::nullopt);
    snapshot.waveMusic = std::exchange(_waveMusic, {});
    snapshot.gameOver = std::exchange(_outcome, std::nullopt);
    if (snapshot.gameOver) {
        snapshot.players = _world.players();
    }
    return {std::move(snapshot)};
}

} // namespace rtype::client
