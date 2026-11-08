#include "Game/Interpolation.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace rtype::client
{

namespace
{

constexpr double TicksPerSecond = game::TickRate;

game::EntityState between(const game::EntityState &from, const game::EntityState &to, float fraction)
{
    game::EntityState state = to;
    state.x = from.x + ((to.x - from.x) * fraction);
    state.y = from.y + ((to.y - from.y) * fraction);
    return state;
}

game::EntityState ahead(const game::EntityState &from, float seconds)
{
    game::EntityState state = from;
    state.x += from.velocityX * seconds;
    state.y += from.velocityY * seconds;
    return state;
}

} // namespace

void Interpolation::receive(std::uint32_t tick, const std::map<std::uint32_t, game::EntityState> &entities,
                            std::vector<game::PlayerState> players)
{
    if (!_worlds.empty() && tick < _worlds.back().tick) {
        return;
    }
    if (!_worlds.empty() && tick == _worlds.back().tick) {
        _worlds.back().entities = entities;
        _worlds.back().players = std::move(players);
        return;
    }
    arrived(tick);
    _worlds.push_back({tick, entities, std::move(players)});
    if (_worlds.size() > MaxWorlds) {
        _worlds.pop_front();
    }
}

void Interpolation::destroyed(std::uint32_t id, std::uint32_t tick)
{
    _destroyed.insert_or_assign(id, tick);
}

void Interpolation::arrived(std::uint32_t tick)
{
    const std::uint32_t gap =
        _arrivals.empty() || tick <= _arrivals.back().tick ? 0 : tick - _arrivals.back().tick;
    _arrivals.push_back({_time, tick, gap});
    while (_arrivals.front().time < _time - Window) {
        _arrivals.pop_front();
    }
}

double Interpolation::delay() const
{
    if (_arrivals.size() < MinArrivals) {
        return DefaultDelay;
    }
    // How late each world came, against the time of its tick: the earliest is the network without jitter
    const auto lateness = [](const Arrival &arrival) {
        return arrival.time - (static_cast<double>(arrival.tick) / TicksPerSecond);
    };
    double earliest = lateness(_arrivals.front());
    for (const Arrival &arrival : _arrivals) {
        earliest = std::min(earliest, lateness(arrival));
    }
    // What each one needed not to be waited for: the ticks since the previous one, its jitter, a tick of
    // margin
    double needed = 0.0;
    for (const Arrival &arrival : _arrivals) {
        const double wait =
            (static_cast<double>(arrival.gap) / TicksPerSecond) + (lateness(arrival) - earliest);
        needed = std::max(needed, wait + (1.0 / TicksPerSecond));
    }
    return std::clamp(needed, MinDelay, MaxDelay);
}

double Interpolation::target() const
{
    return static_cast<double>(_worlds.back().tick) - (delay() * TicksPerSecond);
}

void Interpolation::advance(double elapsed)
{
    _time += std::max(0.0, elapsed);
    if (_worlds.empty()) {
        return;
    }
    const double goal = target();
    if (!_drawn) {
        _drawn = goal;
    } else {
        const double time = std::max(0.0, elapsed);
        const double gap = goal - (*_drawn + (time * TicksPerSecond));
        if (gap > MaxGap * TicksPerSecond) {
            // Far behind (a pause, a burst of snapshots after a silence): too far to catch up smoothly
            *_drawn = goal;
        } else {
            // A little faster when behind, slower when ahead: never backwards, the entities would go back
            const double step = (time * TicksPerSecond) + (gap * std::min(1.0, time * Correction));
            *_drawn += std::max(0.0, step);
        }
    }

    // Forgets the worlds no longer needed: the one just before the drawn tick is kept, to interpolate from
    while (_worlds.size() > 1 && static_cast<double>(_worlds[1].tick) <= *_drawn) {
        _worlds.pop_front();
    }
    std::erase_if(_destroyed,
                  [this](const auto &entry) { return static_cast<double>(entry.second) < *_drawn - 1.0; });
}

game::Snapshot Interpolation::view() const
{
    game::Snapshot snapshot;
    if (_worlds.empty() || !_drawn) {
        return snapshot;
    }
    const double drawn = *_drawn;
    snapshot.tick = static_cast<std::uint32_t>(std::max(0.0, std::floor(drawn)));

    // The worlds around the drawn tick: `from` at or before it, `to` after it
    const auto after = std::ranges::upper_bound(
        _worlds, drawn, std::less<>(), [](const World &world) { return static_cast<double>(world.tick); });
    // Before the oldest world, it is drawn as it is
    const bool beforeAll = after == _worlds.begin();
    const World &from = beforeAll ? _worlds.front() : *std::prev(after);
    const World *to = (beforeAll || after == _worlds.end()) ? nullptr : &*after;
    snapshot.players = from.players;

    const auto gone = [&](std::uint32_t id) {
        const auto destroyed = _destroyed.find(id);
        return destroyed != _destroyed.end() && static_cast<double>(destroyed->second) <= drawn;
    };

    for (const auto &[id, entity] : from.entities) {
        if (gone(id)) {
            continue;
        }
        if (to == nullptr) {
            // Past the newest world: on with its velocity, a while at most
            const double seconds =
                std::clamp((drawn - static_cast<double>(from.tick)) / TicksPerSecond, 0.0, MaxExtrapolation);
            snapshot.entities.push_back(ahead(entity, static_cast<float>(seconds)));
            continue;
        }
        const auto next = to->entities.find(id);
        if (next == to->entities.end()) {
            // Destroyed before the next world: where it was, until its destruction is reached
            snapshot.entities.push_back(entity);
            continue;
        }
        const double span = static_cast<double>(to->tick - from.tick);
        const double fraction = std::clamp((drawn - static_cast<double>(from.tick)) / span, 0.0, 1.0);
        snapshot.entities.push_back(between(entity, next->second, static_cast<float>(fraction)));
    }
    return snapshot;
}

} // namespace rtype::client
