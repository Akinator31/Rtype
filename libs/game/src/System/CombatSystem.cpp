#include "rtype/game/System/CombatSystem.hpp"

#include "rtype/game/Entities.hpp"
#include "rtype/game/Hitbox.hpp"

#include <algorithm>

namespace rtype::game
{

CombatSystem::CombatSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context)
    : _ecs(ecs), _context(context), _owners(use<Owner>(caches)), _positions(use<ecs::Position>(caches)),
      _colliders(use<physics::Collider>(caches)), _healths(use<Health>(caches)),
      _damages(use<Damage>(caches)), _invulnerables(use<Invulnerable>(caches)),
      _playerShips(use<PlayerShip>(caches)), _enemies(use<Enemy>(caches)), _orders(use<EnemyOrders>(caches))
{
}

void CombatSystem::update(float)
{
    const Targets targets = gatherTargets();
    // Hit once per tick: what a shot spent or killed is not hit again before it is destroyed
    std::unordered_set<ecs::entity_t> spent;
    hitEnemies(targets, spent);
    hitShips(targets, spent);
    tickInvulnerability(targets);
    respawnShips();
    endGameWhenDefeated();
}

CombatSystem::Targets CombatSystem::gatherTargets()
{
    Targets targets;
    for (ecs::entity_t entity = 0; entity < _colliders.size(); ++entity) {
        if (_colliders.has(entity) && _positions.has(entity) && _ecs.isAlive(entity)) {
            sortTarget(entity, targets);
        }
    }
    return targets;
}

void CombatSystem::sortTarget(ecs::entity_t entity, Targets &targets) const
{
    if (_playerShips.has(entity) && _healths.has(entity)) {
        targets.ships.push_back(entity);
        return;
    }
    if (_enemies.has(entity) && _healths.has(entity)) {
        targets.enemies.push_back(entity);
    }
    if (!_damages.has(entity)) {
        return;
    }
    const bool fromPlayer = _owners.has(entity) && _owners.get(entity).slot < MaxPlayers;
    (fromPlayer ? targets.playerShots : targets.dangers).push_back(entity);
}

bool CombatSystem::overlap(ecs::entity_t first, ecs::entity_t second) const
{
    return overlaps(_positions.get(first), _colliders.get(first), _positions.get(second),
                    _colliders.get(second));
}

std::optional<ecs::entity_t> CombatSystem::findTouching(ecs::entity_t entity,
                                                        const std::vector<ecs::entity_t> &candidates,
                                                        const std::unordered_set<ecs::entity_t> &spent) const
{
    for (const ecs::entity_t candidate : candidates) {
        if (!spent.contains(candidate) && overlap(entity, candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

void CombatSystem::hitEnemies(const Targets &targets, std::unordered_set<ecs::entity_t> &spent)
{
    for (const ecs::entity_t shot : targets.playerShots) {
        const auto enemy = findTouching(shot, targets.enemies, spent);
        if (!enemy) {
            continue;
        }
        spent.insert(shot);
        destroyLater(shot, DestroyCause::Killed);
        if (isGuarding(*enemy)) {
            continue;
        }
        if (hurt(*enemy, shot) && !startNextPhase(*enemy)) {
            spent.insert(*enemy);
            destroyLater(*enemy, DestroyCause::Killed);
            rewardKill(shot, *enemy);
        }
    }
}

void CombatSystem::hitShips(const Targets &targets, std::unordered_set<ecs::entity_t> &spent)
{
    if (_context.rescue.stage != RescueStage::None) {
        return;
    }
    for (const ecs::entity_t ship : targets.ships) {
        if (_invulnerables.has(ship) && _invulnerables.get(ship).ticks > 0) {
            continue;
        }
        const auto danger = findTouching(ship, targets.dangers, spent);
        if (!danger) {
            continue;
        }
        // A shot is spent, a Bydo keeps going
        if (!_enemies.has(*danger)) {
            spent.insert(*danger);
            destroyLater(*danger, DestroyCause::Killed);
        }
        if (hurt(ship, *danger)) {
            loseShip(ship);
        } else {
            protectShip(ship);
        }
    }
}

bool CombatSystem::hurt(ecs::entity_t target, ecs::entity_t source)
{
    Health &health = _healths.get(target);
    health.points -= std::min(health.points, _damages.get(source).points);
    return health.points == 0;
}

bool CombatSystem::isGuarding(ecs::entity_t enemy) const
{
    return _orders.has(enemy) &&
           (_orders.get(enemy).stance == EnemyStance::Guard || _orders.get(enemy).invulnerable);
}

bool CombatSystem::startNextPhase(ecs::entity_t enemy)
{
    Enemy &state = _enemies.get(enemy);
    const EnemyDefinition &definition = _context.enemies.getDefinition(state.id);
    if (state.phase >= definition.phases) {
        return false;
    }
    ++state.phase;
    _healths.get(enemy).points = definition.health;
    return true;
}

void CombatSystem::rewardKill(ecs::entity_t shot, ecs::entity_t enemy)
{
    const std::uint8_t slot = _owners.get(shot).slot;
    PlayerSlot &player = _context.players[slot];
    if (player.present) {
        player.score += _context.enemies.getDefinition(_enemies.get(enemy).id).score;
    }
}

void CombatSystem::protectShip(ecs::entity_t ship)
{
    const std::uint32_t ticks = _context.rules.hitInvulnerableTicks;
    if (_invulnerables.has(ship)) {
        _invulnerables.get(ship).ticks = ticks;
        return;
    }
    // Added after the systems: it could grow the cache being read
    const ecs::EntityRef reference = _ecs.ref(ship);
    _ecs.defer([this, reference, ticks] {
        if (_ecs.isAlive(reference)) {
            _ecs.addComponent<Invulnerable>(reference.id, ticks);
        }
    });
}

void CombatSystem::loseShip(ecs::entity_t ship)
{
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
    if (!anyone || someoneLeft || _context.over || _context.rescue.stage != RescueStage::None) {
        return;
    }
    if (canStartRescue()) {
        _context.rescue = {};
        _context.rescue.stage = RescueStage::Voice;
        return;
    }
    _context.over = true;
    _context.gameOver = GameOutcome::Defeat;
}

bool CombatSystem::canStartRescue() const
{
    if (!_context.rules.rescue || _context.wave.number != MaxWaveDifficulty) {
        return false;
    }
    for (ecs::entity_t entity = 0; entity < _enemies.size(); ++entity) {
        if (_enemies.has(entity) && _ecs.isAlive(entity) && _healths.has(entity) &&
            _healths.get(entity).points > 0 && _context.enemies.getDefinition(_enemies.get(entity).id).boss) {
            return true;
        }
    }
    return false;
}

} // namespace rtype::game
