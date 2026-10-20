#include "rtype/game/System/CombatSystem.hpp"

#include "rtype/game/Entities.hpp"

#include <algorithm>
#include <cmath>
#include <variant>

namespace rtype::game
{

namespace
{

physics::Vec3 halfSizeOf(const physics::Collider &collider)
{
    return std::get<physics::Aabb>(collider.shape).halfSize;
}

} // namespace

CombatSystem::CombatSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context)
    : _ecs(ecs), _context(context), _kinds(use<Kind>(caches)), _positions(use<ecs::Position>(caches)),
      _colliders(use<physics::Collider>(caches)), _healths(use<Health>(caches)),
      _invulnerables(use<Invulnerable>(caches)), _playerShips(use<PlayerShip>(caches))
{
}

void CombatSystem::update(float)
{
    const Targets targets = gatherTargets();
    // Hit once per tick: what a shot or a ship hit is not hit again before it is destroyed
    std::unordered_set<ecs::entity_t> spent;
    hitShips(targets, spent);
    tickInvulnerability(targets);
    respawnShips();
    endGameWhenDefeated();
}

CombatSystem::Targets CombatSystem::gatherTargets()
{
    Targets targets;
    for (ecs::entity_t entity = 0; entity < _kinds.size(); ++entity) {
        if (!_kinds.has(entity) || !_positions.has(entity) || !_colliders.has(entity) ||
            !_ecs.isAlive(entity)) {
            continue;
        }
        switch (_kinds.get(entity).value) {
        case EntityKind::BydoShot:
            targets.dangers.push_back(entity);
            break;
        case EntityKind::PlayerShip:
            if (_playerShips.has(entity)) {
                targets.ships.push_back(entity);
            }
            break;
        default:
            break;
        }
    }
    return targets;
}

bool CombatSystem::overlap(ecs::entity_t first, ecs::entity_t second) const
{
    const ecs::Position &a = _positions.get(first);
    const ecs::Position &b = _positions.get(second);
    const physics::Vec3 sizeA = halfSizeOf(_colliders.get(first));
    const physics::Vec3 sizeB = halfSizeOf(_colliders.get(second));
    return std::abs(a.x - b.x) < sizeA.x + sizeB.x && std::abs(a.y - b.y) < sizeA.y + sizeB.y;
}

void CombatSystem::hitShips(const Targets &targets, std::unordered_set<ecs::entity_t> &spent)
{
    for (const ecs::entity_t ship : targets.ships) {
        if (_invulnerables.has(ship) && _invulnerables.get(ship).ticks > 0) {
            continue;
        }
        const auto danger = findDanger(ship, targets, spent);
        if (!danger) {
            continue;
        }
        spent.insert(*danger);
        destroyLater(*danger, DestroyCause::Removed);
        loseShip(ship);
    }
}

std::optional<ecs::entity_t> CombatSystem::findDanger(ecs::entity_t ship, const Targets &targets,
                                                      const std::unordered_set<ecs::entity_t> &spent) const
{
    for (const ecs::entity_t danger : targets.dangers) {
        if (!spent.contains(danger) && overlap(ship, danger)) {
            return danger;
        }
    }
    return std::nullopt;
}

void CombatSystem::loseShip(ecs::entity_t ship)
{
    Health &health = _healths.get(ship);
    health.points = static_cast<std::uint8_t>(health.points > 0 ? health.points - 1 : 0);
    if (health.points > 0) {
        return;
    }
    destroyLater(ship, DestroyCause::Killed);
    PlayerSlot &player = _context.players[_playerShips.get(ship).slot];
    player.lives = static_cast<std::uint8_t>(player.lives > 0 ? player.lives - 1 : 0);
    player.respawnIn = player.lives > 0 ? _context.rules.respawnDelay : 0;
}

void CombatSystem::destroyLater(ecs::entity_t entity, DestroyCause cause)
{
    const ecs::EntityRef reference = _ecs.ref(entity);
    _ecs.defer([this, reference, cause] {
        if (_ecs.isAlive(reference)) {
            destroyEntity(_ecs, _context, reference.id, cause);
        }
    });
}

void CombatSystem::tickInvulnerability(const Targets &targets)
{
    for (const ecs::entity_t ship : targets.ships) {
        if (_invulnerables.has(ship) && _invulnerables.get(ship).ticks > 0) {
            --_invulnerables.get(ship).ticks;
        }
    }
}

void CombatSystem::respawnShips()
{
    for (std::size_t slot = 0; slot < MaxPlayers; ++slot) {
        PlayerSlot &player = _context.players[slot];
        if (!player.present || player.respawnIn == 0 || --player.respawnIn > 0) {
            continue;
        }
        _ecs.defer([this, slot = static_cast<std::uint8_t>(slot)] {
            const ecs::entity_t ship = spawnPlayerShip(_ecs, _context, slot);
            _ecs.addComponent<Invulnerable>(ship, _context.rules.invulnerableTicks);
        });
    }
}

void CombatSystem::endGameWhenDefeated()
{
    const auto &players = _context.players;
    const bool anyone = std::ranges::any_of(players, [](const PlayerSlot &player) { return player.present; });
    const bool someoneLeft = std::ranges::any_of(
        players, [](const PlayerSlot &player) { return player.present && player.lives > 0; });
    if (anyone && !someoneLeft && !_over) {
        _over = true;
        _context.gameOver = GameOutcome::Defeat;
    }
}

} // namespace rtype::game
