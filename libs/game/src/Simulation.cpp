#include "rtype/game/Simulation.hpp"

#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/Entities.hpp"
#include "rtype/game/System/AreaSystem.hpp"
#include "rtype/game/System/PlayerControlSystem.hpp"
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

Simulation::Simulation(Rules rules)
{
    _context.rules = rules;
    // The order of a tick: the inputs, the movement, the walls, then what left the area
    _ecs.registerSystem<PlayerControlSystem>(_ecs, _context);
    _ecs.registerSystem<physics::MovementSystem>();
    _ecs.registerSystem<physics::BoundsSystem>(_ecs);
    _ecs.registerSystem<AreaSystem>(_ecs, _context);
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
