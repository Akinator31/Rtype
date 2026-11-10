#include "Game/LocalSession.hpp"

#include "rtype/game/Component/Components.hpp"
#include "rtype/game/Constants.hpp"
#include "rtype/game/ShipMovement.hpp"

#include <algorithm>
#include <random>
#include <utility>

namespace rtype::client
{

namespace
{

constexpr double Tick = 1.0 / game::TickRate;

/// The rules of a game offline: the story mode, where allies come to the rescue against the final boss. Each
/// game draws its own seed, so the waves are not the same every time.
game::Rules storyRules(game::Rules rules)
{
    rules.rescue = true;
    rules.seed = std::random_device{}();
    return rules;
}

} // namespace

LocalSession::LocalSession(game::Rules rules, const game::GameConfig &config)
    : _simulation(storyRules(rules), config), _waveCount(static_cast<std::uint16_t>(config.waves.size()))
{
    _simulation.addPlayer(localSlot());
}

void LocalSession::update(float elapsed)
{
    if (_paused) {
        return;
    }
    _pending += std::max(0.0f, elapsed);
    int steps = 0;
    while (_pending >= Tick && steps < MaxStepsPerUpdate) {
        _simulation.setInput(localSlot(), _input);
        _simulation.step();
        _snapshots.push_back(_simulation.snapshot());
        _last = _simulation.snapshot();
        _over = _over || _simulation.snapshot().gameOver.has_value();
        _pending -= Tick;
        ++steps;
    }
    if (steps == MaxStepsPerUpdate) {
        // Too late to catch up: the rest is forgotten
        _pending = std::min(_pending, Tick);
    }
}

std::optional<physics::Collider> LocalSession::findCollider(const game::EntityState &state)
{
    ecs::Ecs &world = _simulation.ecs();
    const auto &ids = world.cache<game::NetworkId>();
    const auto &colliders = world.cache<physics::Collider>();
    for (ecs::entity_t entity = 0; entity < ids.size(); ++entity) {
        if (ids.has(entity) && ids.get(entity).value == state.id) {
            return colliders.has(entity) ? std::optional(colliders.get(entity)) : std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<game::Snapshot> LocalSession::between() const
{
    if (_paused || !_last) {
        return std::nullopt;
    }
    const auto fraction = static_cast<float>(std::clamp(_pending / Tick, 0.0, 1.0));
    game::Snapshot drawn;
    drawn.tick = _last->tick;
    drawn.players = _last->players;
    drawn.rescue = _last->rescue;
    drawn.entities = _last->entities;
    for (game::EntityState &entity : drawn.entities) {
        if (entity.kind == game::EntityKind::PlayerShip && entity.owner == localSlot() &&
            _last->rescue == game::RescueStage::None) {
            // As the next step will move it, walls included. Not during the rescue, which flies it on its own
            const game::Rules &rules = _simulation.rules();
            const ecs::Position next = game::moveShip({entity.x, entity.y}, _input, rules);
            entity.x += (next.x - entity.x) * fraction;
            entity.y += (next.y - entity.y) * fraction;
            continue;
        }
        entity.x += entity.velocityX * game::TickDuration * fraction;
        entity.y += entity.velocityY * game::TickDuration * fraction;
    }
    return drawn;
}

std::vector<game::Snapshot> LocalSession::takeSnapshots()
{
    return std::exchange(_snapshots, {});
}

} // namespace rtype::client
