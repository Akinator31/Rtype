#include "Game/LocalSession.hpp"

#include "rtype/game/Component/Components.hpp"
#include "rtype/game/Constants.hpp"

#include <algorithm>
#include <utility>

namespace rtype::client
{

namespace
{

constexpr double Tick = 1.0 / game::TickRate;

/// The rules of a game offline: the story mode, where allies come to the rescue against the final boss.
game::Rules storyRules(game::Rules rules)
{
    rules.rescue = true;
    return rules;
}

} // namespace

LocalSession::LocalSession(game::Rules rules, const game::GameConfig &config)
    : _simulation(storyRules(rules), config)
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

std::vector<game::Snapshot> LocalSession::takeSnapshots()
{
    return std::exchange(_snapshots, {});
}

} // namespace rtype::client
