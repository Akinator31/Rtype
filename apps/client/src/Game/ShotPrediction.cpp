#include "Game/ShotPrediction.hpp"

#include "rtype/game/Constants.hpp"
#include "rtype/game/Projectile/ProjectileBehavior.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace rtype::client
{

namespace
{

/// Ticks a predicted shot is followed for at most: far longer than it takes to cross the game area.
constexpr std::uint32_t MaxSteps = 1200;

} // namespace

bool ShotPrediction::isShotOf(const game::EntityState &entity, std::uint8_t slot)
{
    return entity.owner == slot && (entity.kind == game::EntityKind::PlayerShot ||
                                    entity.kind == game::EntityKind::PlayerChargedShot);
}

void ShotPrediction::arm(game::ProjectileCategory mechanic, game::Rules rules)
{
    _weapon = Weapon{mechanic, rules};
}

void ShotPrediction::received(const game::EntityState &shot, std::uint32_t tick)
{
    if (shot.age == 255 || shot.age > tick || _aliases.contains(shot.id)) {
        // Too old to tell when it was fired, or already matched
        return;
    }
    const auto ghost = _ghosts.find(tick - shot.age);
    if (ghost == _ghosts.end() || ghost->second.server) {
        return;
    }
    ghost->second.server = shot.id;
    _aliases.emplace(shot.id, ghost->second.id);
}

void ShotPrediction::destroyed(std::uint32_t id, std::uint32_t tick)
{
    _ends.try_emplace(id, End{tick, false});
}

void ShotPrediction::predict(const std::map<std::uint32_t, game::ShotLaunch> &shots, std::uint32_t confirmed)
{
    for (const auto &[tick, launch] : shots) {
        const auto ghost = _ghosts.find(tick);
        if (ghost == _ghosts.end()) {
            _ghosts.emplace(tick, Ghost{_nextId++, launch, std::nullopt, false});
        } else if (!ghost->second.server) {
            // Predicted again from a snapshot: the ship was elsewhere, or aimed another way
            ghost->second.launch = launch;
        }
    }
    for (auto ghost = _ghosts.begin(); ghost != _ghosts.end();) {
        const std::uint32_t tick = ghost->first;
        const bool unmatched = !ghost->second.server;
        // No longer predicted: the game did not fire then. Not sent by the game in time: lost, or destroyed
        // before a snapshot had it
        const bool mispredicted = unmatched && tick > confirmed && !shots.contains(tick);
        const bool unconfirmed = unmatched && tick + Grace <= confirmed;
        if (mispredicted || unconfirmed) {
            const auto next = std::next(ghost);
            forget(ghost);
            ghost = next;
        } else {
            ++ghost;
        }
    }
}

game::EntityState ShotPrediction::place(const Ghost &ghost, std::uint32_t fired, double tick,
                                        std::uint8_t slot) const
{
    game::EntityState state;
    state.id = ghost.id;
    state.kind = game::EntityKind::PlayerShot;
    state.owner = slot;
    const double since = std::clamp(tick - static_cast<double>(fired), 0.0, static_cast<double>(MaxSteps));
    const auto steps = static_cast<std::uint32_t>(since);
    ecs::Position position = ghost.launch.position;
    physics::Velocity velocity = ghost.launch.velocity;
    if (_weapon) {
        for (std::uint32_t age = 0; age < steps; ++age) {
            position =
                game::moveShot(position, _weapon->mechanic, ghost.launch.velocity, age, _weapon->rules);
        }
        if (steps > 0) {
            velocity = game::getProjectileVelocity(_weapon->mechanic, ghost.launch.velocity, steps - 1,
                                                   _weapon->rules);
        }
        // Between two ticks, on the way to the next one: drawn at the frame rate
        const auto part = static_cast<float>(since - static_cast<double>(steps));
        if (part > 0.0f) {
            const ecs::Position next =
                game::moveShot(position, _weapon->mechanic, ghost.launch.velocity, steps, _weapon->rules);
            position.x += (next.x - position.x) * part;
            position.y += (next.y - position.y) * part;
        }
    }
    state.x = position.x;
    state.y = position.y;
    state.velocityX = velocity.x;
    state.velocityY = velocity.y;
    state.age = static_cast<std::uint8_t>(std::min(steps, 255U));
    return state;
}

std::optional<double> ShotPrediction::targetOf(std::uint32_t id, double ship)
{
    const auto end = _ends.find(id);
    if (end == _ends.end() || ship < end->second.tick) {
        return ship;
    }
    if (end->second.shown) {
        return std::nullopt;
    }
    // Where the game destroyed it, once: the impact is drawn there
    end->second.shown = true;
    return end->second.tick;
}

void ShotPrediction::forget(std::map<std::uint32_t, Ghost>::iterator ghost)
{
    if (ghost->second.server) {
        _aliases.erase(*ghost->second.server);
    }
    _ghosts.erase(ghost);
}

void ShotPrediction::apply(game::Snapshot &snapshot, std::uint8_t slot, double drawn, double ship)
{
    const auto ghostOf = [this](std::uint32_t id) {
        const auto alias = _aliases.find(id);
        return alias == _aliases.end() ? _ghosts.end()
                                       : std::ranges::find_if(_ghosts, [&](const auto &entry) {
                                             return entry.second.id == alias->second;
                                         });
    };

    // The shots of the game: past their end, gone; predicted, on the path of their prediction; else moved on
    std::set<std::uint32_t> drawnGhosts;
    std::vector<game::EntityState> entities;
    entities.reserve(snapshot.entities.size() + _ghosts.size());
    for (game::EntityState &entity : snapshot.entities) {
        if (!isShotOf(entity, slot)) {
            entities.push_back(entity);
            continue;
        }
        const std::optional<double> target = targetOf(entity.id, ship);
        if (!target) {
            continue;
        }
        if (const auto ghost = ghostOf(entity.id); ghost != _ghosts.end()) {
            ghost->second.seen = true;
            drawnGhosts.insert(ghost->second.id);
            entities.push_back(place(ghost->second, ghost->first, *target, slot));
            continue;
        }
        const auto seconds = static_cast<float>(std::max(0.0, *target - drawn) / game::TickRate);
        entity.x += entity.velocityX * seconds;
        entity.y += entity.velocityY * seconds;
        entities.push_back(entity);
    }

    // The predicted shots the world does not draw yet: from the moment they are fired
    for (auto ghost = _ghosts.begin(); ghost != _ghosts.end();) {
        Ghost &predicted = ghost->second;
        if (drawnGhosts.contains(predicted.id)) {
            ++ghost;
            continue;
        }
        if (predicted.server && predicted.seen) {
            // The shot of the game is no longer drawn: done
            const auto next = std::next(ghost);
            forget(ghost);
            ghost = next;
            continue;
        }
        const std::optional<double> target =
            predicted.server ? targetOf(*predicted.server, ship) : std::optional<double>(ship);
        if (!target) {
            const auto next = std::next(ghost);
            forget(ghost);
            ghost = next;
            continue;
        }
        if (static_cast<double>(ghost->first) <= *target) {
            entities.push_back(place(predicted, ghost->first, *target, slot));
        }
        ++ghost;
    }
    snapshot.entities = std::move(entities);

    // The interpolation no longer draws them once the drawn tick passes their destruction
    std::erase_if(
        _ends, [drawn](const auto &entry) { return static_cast<double>(entry.second.tick) < drawn - 1.0; });
}

std::uint32_t ShotPrediction::idOf(std::uint32_t id) const
{
    const auto alias = _aliases.find(id);
    return alias == _aliases.end() ? id : alias->second;
}

} // namespace rtype::client
