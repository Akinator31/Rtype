#include "rtype/game/Simulation.hpp"

#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/Entities.hpp"
#include "rtype/game/System/AreaSystem.hpp"
#include "rtype/game/System/CombatSystem.hpp"
#include "rtype/game/System/EnemySystem.hpp"
#include "rtype/game/System/PlayerControlSystem.hpp"
#include "rtype/game/System/ProjectileSystem.hpp"
#include "rtype/game/System/WaveSystem.hpp"
#include "rtype/game/System/WeaponSystem.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/physics/Component/Motion.hpp"
#include "rtype/physics/System/BoundsSystem.hpp"
#include "rtype/physics/System/MovementSystem.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

namespace rtype::game
{

namespace
{

const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("game");
    return instance;
}

} // namespace

Simulation::Simulation(Rules rules, const GameConfig &config)
{
    _context.rules = rules;
    _context.seedRandom();
    registerConfig(config);
    // The order of a tick: the inputs, the waves, the Bydos, the weapons, the projectiles, the movement, the
    // walls, the hits, then what left the area
    _ecs.registerSystem<PlayerControlSystem>(_ecs, _context);
    _ecs.registerSystem<WaveSystem>(_ecs, _context);
    _ecs.registerSystem<EnemySystem>(_ecs, _context);
    _ecs.registerSystem<WeaponSystem>(_ecs, _context);
    _ecs.registerSystem<ProjectileSystem>(_ecs, _context);
    _ecs.registerSystem<physics::MovementSystem>();
    _ecs.registerSystem<physics::BoundsSystem>(_ecs);
    _ecs.registerSystem<CombatSystem>(_ecs, _context);
    _ecs.registerSystem<AreaSystem>(_ecs, _context);
}

void Simulation::registerConfig(const GameConfig &config)
{
    for (const ProjectileDefinition &projectile : config.projectiles) {
        _context.projectiles.addDefinition(projectile);
    }
    for (const WeaponDefinition &weapon : config.weapons) {
        _context.weapons.addDefinition(weapon);
    }
    for (const ShipDefinition &ship : config.ships) {
        _context.ships.addDefinition(ship);
    }
    for (const EnemyDefinition &enemy : config.enemies) {
        _context.enemies.addDefinition(enemy);
    }
    for (const WaveDefinition &wave : config.waves) {
        _context.waves.addDefinition(wave, _context.enemies);
    }
    registerPlayer(config.player);
}

void Simulation::registerPlayer(const PlayerDefinition &player)
{
    if (!player.ship.empty() && !_context.ships.hasDefinition(player.ship)) {
        throw std::invalid_argument(std::format("the player uses the unknown ship '{}'", player.ship));
    }
    if (!player.weapon.empty() && !_context.weapons.hasDefinition(player.weapon)) {
        throw std::invalid_argument(std::format("the player uses the unknown weapon '{}'", player.weapon));
    }
    if (player.health == 0) {
        throw std::invalid_argument("the player needs health");
    }
    _context.player = player;
}

void Simulation::addPlayer(std::uint8_t slot)
{
    if (slot >= MaxPlayers) {
        throw std::invalid_argument(std::format("a player slot is 0 to {}, not {}", MaxPlayers - 1, slot));
    }
    PlayerSlot &player = _context.players[slot];
    if (player.present) {
        throw std::invalid_argument(std::format("the player slot {} is taken", slot));
    }
    player = {true, {}, _context.rules.lives, 0};
    spawnPlayerShip(_ecs, _context, slot);
    logger().debug("Player {} joined at tick {}", slot, _context.tick);
}

void Simulation::removePlayer(std::uint8_t slot)
{
    if (!hasPlayer(slot)) {
        return;
    }
    if (const auto ship = findPlayerShip(_ecs, slot)) {
        destroyEntity(_ecs, _context, *ship, DestroyCause::Removed);
    }
    _context.players[slot] = {};
    logger().debug("Player {} left at tick {}", slot, _context.tick);
}

bool Simulation::hasPlayer(std::uint8_t slot) const
{
    return slot < MaxPlayers && _context.players[slot].present;
}

std::size_t Simulation::playerCount() const
{
    return static_cast<std::size_t>(
        std::ranges::count_if(_context.players, [](const PlayerSlot &player) { return player.present; }));
}

void Simulation::setInput(std::uint8_t slot, PlayerInput input)
{
    if (hasPlayer(slot)) {
        _context.players[slot].input = input;
    }
}

void Simulation::step()
{
    ++_context.tick;
    _ecs.update(TickDuration);
    buildSnapshot();
}

void Simulation::buildSnapshot()
{
    Snapshot snapshot;
    snapshot.tick = _context.tick;

    for (std::size_t slot = 0; slot < MaxPlayers; ++slot) {
        const PlayerSlot &player = _context.players[slot];
        if (player.present) {
            snapshot.players.push_back({static_cast<std::uint8_t>(slot), player.lives, player.score});
        }
    }

    ecs::ComponentCache<NetworkId> &ids = _ecs.cache<NetworkId>();
    ecs::ComponentCache<Kind> &kinds = _ecs.cache<Kind>();
    ecs::ComponentCache<ecs::Position> &positions = _ecs.cache<ecs::Position>();
    ecs::ComponentCache<physics::Velocity> &velocities = _ecs.cache<physics::Velocity>();
    ecs::ComponentCache<Owner> &owners = _ecs.cache<Owner>();
    ecs::ComponentCache<Health> &healths = _ecs.cache<Health>();
    ecs::ComponentCache<Invulnerable> &invulnerables = _ecs.cache<Invulnerable>();
    ecs::ComponentCache<PlayerShip> &ships = _ecs.cache<PlayerShip>();
    for (ecs::entity_t entity = 0; entity < ids.size(); ++entity) {
        if (!ids.has(entity) || !kinds.has(entity) || !positions.has(entity) || !_ecs.isAlive(entity)) {
            continue;
        }
        EntityState state;
        state.id = ids.get(entity).value;
        state.kind = kinds.get(entity).value;
        state.owner = owners.has(entity) ? owners.get(entity).slot : NoOwner;
        state.x = positions.get(entity).x;
        state.y = positions.get(entity).y;
        if (velocities.has(entity)) {
            state.velocityX = velocities.get(entity).x;
            state.velocityY = velocities.get(entity).y;
        }
        state.health = healths.has(entity) ? healths.get(entity).points : 0;
        if (invulnerables.has(entity) && invulnerables.get(entity).ticks > 0) {
            state.flags |= EntityState::Invulnerable;
        }
        if (ships.has(entity)) {
            const int aim = _context.players[ships.get(entity).slot].input.aim();
            if (aim < 0) {
                state.flags |= EntityState::AimingUp;
            } else if (aim > 0) {
                state.flags |= EntityState::AimingDown;
            }
        }
        snapshot.entities.push_back(state);
    }
    // By id, whatever the reuse of the ids of the ECS: the same game always gives the same snapshot
    std::ranges::sort(snapshot.entities, {}, &EntityState::id);

    snapshot.destroyed = std::exchange(_context.destroyed, {});
    snapshot.waveStarted = std::exchange(_context.waveStarted, std::nullopt);
    snapshot.gameOver = std::exchange(_context.gameOver, std::nullopt);
    _snapshot = std::move(snapshot);
}

} // namespace rtype::game
