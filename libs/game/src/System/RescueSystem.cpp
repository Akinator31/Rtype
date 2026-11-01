#include "rtype/game/System/RescueSystem.hpp"

#include "rtype/game/Entities.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace rtype::game
{

namespace
{

struct Spot {
    float x = 0.0f;
    float y = 0.0f;
};

/// The arrowhead of the formation, in game units: the player at its tip, the allies behind, from the nearest.
constexpr std::array<Spot, RescueSystem::FormationSize> Formation{{
    {620.0f, 540.0f},
    {470.0f, 360.0f},
    {470.0f, 720.0f},
    {320.0f, 190.0f},
    {320.0f, 890.0f},
    {170.0f, 540.0f},
}};
/// Where the allies appear, left of the game area, each a little further than the previous one.
constexpr float AllyStartX = -160.0f;
constexpr float AllyStartGap = 70.0f;
/// Speed of the allies flying to their place, in game units per second.
constexpr float AllySpeed = 700.0f;

/// Where the boss waits, as in config/enemies/bydo_shogun.lua, and its speed back there.
constexpr Spot BossSpot{1600.0f, 540.0f};
constexpr float BossSpeed = 900.0f;
constexpr float BossReturnSpeed = 1400.0f;
/// Within this distance of a point, an entity is there.
constexpr float Arrival = 5.0f;

/// Where the dashes of the boss may end: its box stays in the game area, as in bydo_shogun.lua.
constexpr float DashMinX = 120.0f;
constexpr float DashMaxX = 1800.0f;
constexpr float DashMinY = 200.0f;
constexpr float DashMaxY = 880.0f;
/// Ticks its blade is raised before the first dash, and between two dashes.
constexpr std::uint32_t FirstWindUp = 45;
constexpr std::uint32_t NextWindUp = 10;
/// Speed of a dash, how far past the ship it goes, the most ticks it lasts, and when it has reached its end.
constexpr float DashSpeed = 2600.0f;
constexpr float DashOvershoot = 160.0f;
constexpr std::uint32_t DashAtMost = 90;
constexpr float DashArrival = 30.0f;
/// A ship hit by a dash is thrown that fast the way the dash goes, for that many ticks, slowing down.
constexpr float KnockbackSpeed = 1100.0f;
constexpr std::uint32_t KnockbackTicks = 24;
constexpr float KnockbackDrag = 0.9f;

/// Ticks between two shots of an ally, during the assault and the combined attack.
constexpr std::uint32_t AssaultInterval = 20;
constexpr std::uint32_t CombinedInterval = 6;
/// The health of the boss drains by steps every that many ticks during the combined attack: each step is a
/// damage number in the client.
constexpr std::uint32_t DrainInterval = 15;

} // namespace

RescueSystem::RescueSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context)
    : _ecs(ecs), _context(context), _enemies(use<Enemy>(caches)), _orders(use<EnemyOrders>(caches)),
      _positions(use<ecs::Position>(caches)), _velocities(use<physics::Velocity>(caches)),
      _healths(use<Health>(caches)), _allies(use<Ally>(caches))
{
    use<PlayerShip>(caches);
}

void RescueSystem::update(float elapsed)
{
    RescueProgress &rescue = _context.rescue;
    if (rescue.stage == RescueStage::None || _context.over) {
        return;
    }
    ++rescue.ticks;
    const std::optional<ecs::entity_t> boss = findBoss();
    if (!boss && rescue.stage != RescueStage::Finished) {
        // Gone without its fall: nothing left to save the player from
        enter(RescueStage::Finished);
    }
    const Rules &rules = _context.rules;
    switch (rescue.stage) {
    case RescueStage::None:
        break;
    case RescueStage::Voice:
        order(*boss, EnemyStance::Guard);
        steer(*boss, BossSpot.x, BossSpot.y, BossSpeed, elapsed);
        if (rescue.ticks >= rules.rescueVoiceTicks) {
            callAllies();
            enter(RescueStage::Arrival);
        }
        break;
    case RescueStage::Arrival:
        order(*boss, EnemyStance::Guard);
        steer(*boss, BossSpot.x, BossSpot.y, BossSpeed, elapsed);
        holdFormation(elapsed, false);
        if (rescue.ticks >= rules.rescueArrivalTicks) {
            enter(RescueStage::Assault);
        }
        break;
    case RescueStage::Assault:
        order(*boss, EnemyStance::Guard);
        steer(*boss, BossSpot.x, BossSpot.y, BossSpeed, elapsed);
        holdFormation(elapsed, true);
        fireAt(*boss, AssaultInterval, false);
        if (rescue.ticks >= rules.rescueAssaultTicks) {
            enter(RescueStage::Dash);
        }
        break;
    case RescueStage::Dash:
        runDash(*boss, elapsed);
        holdFormation(elapsed, false);
        break;
    case RescueStage::Prompt:
        order(*boss, EnemyStance::None);
        steer(*boss, BossSpot.x, BossSpot.y, BossSpeed, elapsed);
        holdFormation(elapsed, false);
        runPrompt();
        break;
    case RescueStage::Combined:
        order(*boss, EnemyStance::None);
        _velocities.get(*boss) = {};
        holdFormation(elapsed, false);
        fireAt(*boss, CombinedInterval, true);
        runCombined(*boss);
        break;
    case RescueStage::Finished:
        holdFormation(elapsed, false);
        if (rescue.ticks >= rules.rescueVictoryDelay) {
            _context.over = true;
            _context.gameOver = GameOutcome::Victory;
        }
        break;
    }
}

void RescueSystem::enter(RescueStage stage)
{
    RescueProgress &rescue = _context.rescue;
    rescue.stage = stage;
    rescue.ticks = 0;
    rescue.target = 0;
    rescue.dashing = false;
    rescue.fireReleased = false;
}

std::optional<ecs::entity_t> RescueSystem::findBoss() const
{
    for (ecs::entity_t entity = 0; entity < _enemies.size(); ++entity) {
        if (_enemies.has(entity) && _ecs.isAlive(entity) && _positions.has(entity) &&
            _velocities.has(entity) &&
            _context.enemies.getDefinition(_enemies.get(entity).id).boss) {
            return entity;
        }
    }
    return std::nullopt;
}

std::optional<std::uint8_t> RescueSystem::findRescuedSlot() const
{
    for (std::size_t slot = 0; slot < MaxPlayers; ++slot) {
        if (_context.players[slot].present) {
            return static_cast<std::uint8_t>(slot);
        }
    }
    return std::nullopt;
}

std::optional<ecs::entity_t> RescueSystem::findShip(std::size_t index) const
{
    if (index == 0) {
        const std::optional<std::uint8_t> slot = findRescuedSlot();
        return slot ? findPlayerShip(_ecs, *slot) : std::nullopt;
    }
    for (ecs::entity_t entity = 0; entity < _allies.size(); ++entity) {
        if (_allies.has(entity) && _ecs.isAlive(entity) && _allies.get(entity).index + 1U == index) {
            return entity;
        }
    }
    return std::nullopt;
}

void RescueSystem::order(ecs::entity_t boss, EnemyStance stance)
{
    if (!_orders.has(boss)) {
        return;
    }
    EnemyOrders &orders = _orders.get(boss);
    orders = {};
    orders.stance = stance;
    orders.invulnerable = true;
}

void RescueSystem::steer(ecs::entity_t entity, float x, float y, float speed, float elapsed)
{
    const ecs::Position &position = _positions.get(entity);
    const float distanceX = x - position.x;
    const float distanceY = y - position.y;
    const float distance = std::hypot(distanceX, distanceY);
    physics::Velocity &velocity = _velocities.get(entity);
    if (distance <= 0.0f || elapsed <= 0.0f) {
        velocity = {};
        return;
    }
    // Slower on the last tick, so that it stops on the point instead of flying past it
    const float pace = std::min(speed, distance / elapsed);
    velocity = {distanceX / distance * pace, distanceY / distance * pace};
}

void RescueSystem::holdFormation(float elapsed, bool playerFree)
{
    RescueProgress &rescue = _context.rescue;
    for (std::size_t index = 0; index < FormationSize; ++index) {
        const std::optional<ecs::entity_t> ship = findShip(index);
        if (!ship || !_velocities.has(*ship) || !_positions.has(*ship)) {
            continue;
        }
        std::uint32_t &stunned = index == 0 ? rescue.playerStunned : _allies.get(*ship).stunned;
        if (stunned > 0) {
            --stunned;
            physics::Velocity &velocity = _velocities.get(*ship);
            velocity = {velocity.x * KnockbackDrag, velocity.y * KnockbackDrag};
            continue;
        }
        if (index == 0 && playerFree) {
            continue;
        }
        const float speed = index == 0 ? _context.rules.shipSpeed : AllySpeed;
        steer(*ship, Formation[index].x, Formation[index].y, speed, elapsed);
    }
}

void RescueSystem::fireAt(ecs::entity_t boss, std::uint32_t interval, bool withPlayer)
{
    const std::optional<std::uint8_t> slot = findRescuedSlot();
    const std::string &weapon = _context.player.weapon;
    if (!slot || weapon.empty()) {
        return;
    }
    const ecs::Position target = _positions.get(boss);
    for (std::size_t index = withPlayer ? 0 : 1; index < FormationSize; ++index) {
        // In turn rather than all at once
        if ((_context.rescue.ticks + (index * 3)) % interval != 0) {
            continue;
        }
        const std::optional<ecs::entity_t> ship = findShip(index);
        if (!ship || !_positions.has(*ship)) {
            continue;
        }
        const ecs::Position nose{_positions.get(*ship).x + _context.rules.shipHalfWidth,
                                 _positions.get(*ship).y};
        // Their shots belong to the player rescued: they hit the Bydos as its own
        _ecs.defer([this, weapon, owner = *slot, nose, target] {
            spawnAimedShot(_ecs, _context, weapon, owner, nose.x, nose.y, target.x, target.y);
        });
    }
}

void RescueSystem::callAllies()
{
    const std::optional<std::uint8_t> slot = findRescuedSlot();
    if (slot) {
        PlayerSlot &player = _context.players[*slot];
        player.lives = std::max<std::uint8_t>(player.lives, 1);
        player.respawnIn = 0;
    }
    _ecs.defer([this, slot] {
        if (slot && !findPlayerShip(_ecs, *slot)) {
            spawnPlayerShip(_ecs, _context, *slot);
        }
        for (std::size_t index = 1; index < FormationSize; ++index) {
            spawnAlly(_ecs, _context, static_cast<std::uint8_t>(index - 1),
                      AllyStartX - (AllyStartGap * static_cast<float>(index)), Formation[index].y);
        }
    });
}

void RescueSystem::runDash(ecs::entity_t boss, float elapsed)
{
    RescueProgress &rescue = _context.rescue;
    order(boss, EnemyStance::Slash);
    if (rescue.target >= FormationSize) {
        // Every ship hit: back to its spot, then the players strike back
        steer(boss, BossSpot.x, BossSpot.y, BossReturnSpeed, elapsed);
        const ecs::Position &position = _positions.get(boss);
        if (std::hypot(BossSpot.x - position.x, BossSpot.y - position.y) <= Arrival) {
            _velocities.get(boss) = {};
            enter(RescueStage::Prompt);
        }
        return;
    }
    const std::optional<ecs::entity_t> ship = findShip(rescue.target);
    if (!ship) {
        ++rescue.target;
        return;
    }
    const ecs::Position &from = _positions.get(boss);
    if (!rescue.dashing) {
        // Blade raised, still, then it dashes where the ship is then
        _velocities.get(boss) = {};
        if (rescue.ticks < (rescue.target == 0 ? FirstWindUp : NextWindUp)) {
            return;
        }
        const ecs::Position &to = _positions.get(*ship);
        const float distance = std::max(1.0f, std::hypot(to.x - from.x, to.y - from.y));
        rescue.dashX = std::clamp(to.x + ((to.x - from.x) / distance * DashOvershoot), DashMinX, DashMaxX);
        rescue.dashY = std::clamp(to.y + ((to.y - from.y) / distance * DashOvershoot), DashMinY, DashMaxY);
        rescue.dashing = true;
        rescue.ticks = 0;
        return;
    }
    const float distanceX = rescue.dashX - from.x;
    const float distanceY = rescue.dashY - from.y;
    const float distance = std::hypot(distanceX, distanceY);
    if (distance > DashArrival && rescue.ticks < DashAtMost) {
        steer(boss, rescue.dashX, rescue.dashY, DashSpeed, elapsed);
        return;
    }
    // Through the ship, which it went past: the ship is thrown the way the dash went, unhurt
    _velocities.get(boss) = {};
    const ecs::Position &shipAt = _positions.get(*ship);
    float pushX = from.x - shipAt.x;
    float pushY = from.y - shipAt.y;
    const float length = std::hypot(pushX, pushY);
    if (length > 0.0f) {
        pushX /= length;
        pushY /= length;
    } else {
        pushX = -1.0f;
        pushY = 0.0f;
    }
    _velocities.get(*ship) = {pushX * KnockbackSpeed, pushY * KnockbackSpeed};
    (rescue.target == 0 ? rescue.playerStunned : _allies.get(*ship).stunned) = KnockbackTicks;
    ++rescue.target;
    rescue.dashing = false;
    rescue.ticks = 0;
}

void RescueSystem::runPrompt()
{
    RescueProgress &rescue = _context.rescue;
    const std::optional<std::uint8_t> slot = findRescuedSlot();
    if (!slot) {
        return;
    }
    const bool fire = _context.players[*slot].input.isHeld(PlayerInput::Fire);
    if (!fire) {
        rescue.fireReleased = true;
    } else if (rescue.fireReleased) {
        const std::optional<ecs::entity_t> boss = findBoss();
        enter(RescueStage::Combined);
        rescue.bossHealth = boss && _healths.has(*boss) ? _healths.get(*boss).points : 0;
    }
}

void RescueSystem::runCombined(ecs::entity_t boss)
{
    RescueProgress &rescue = _context.rescue;
    const std::uint32_t total = std::max<std::uint32_t>(_context.rules.rescueCombinedTicks, 1);
    if (rescue.ticks < total) {
        if (rescue.ticks % DrainInterval == 0 && _healths.has(boss)) {
            const std::uint32_t left = rescue.bossHealth * (total - rescue.ticks) / total;
            _healths.get(boss).points = static_cast<std::uint8_t>(std::max<std::uint32_t>(left, 1));
        }
        return;
    }
    if (_healths.has(boss)) {
        _healths.get(boss).points = 0;
    }
    if (const std::optional<std::uint8_t> slot = findRescuedSlot()) {
        _context.players[*slot].score += _context.enemies.getDefinition(_enemies.get(boss).id).score;
    }
    const ecs::EntityRef reference = _ecs.ref(boss);
    _ecs.defer([this, reference] {
        if (_ecs.isAlive(reference)) {
            destroyEntity(_ecs, _context, reference.id, DestroyCause::Killed);
        }
    });
    enter(RescueStage::Finished);
}

} // namespace rtype::game
