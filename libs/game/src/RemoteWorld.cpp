#include "rtype/game/RemoteWorld.hpp"

#include <map>

namespace rtype::game
{

bool RemoteWorld::apply(const SnapshotPart &part)
{
    if (_received && part.tick < _tick) {
        return false;
    }
    if (!_received || part.tick > _tick) {
        _tick = part.tick;
        _received = true;
        // The snapshots have reached their destruction: no part older than it can come any more
        std::erase_if(_destroyed, [this](const auto &destroyed) { return destroyed.second <= _tick; });
    }
    _lastInput = part.lastInput;
    _players = part.players;

    for (const EntityState &entity : part.entities) {
        if (!isKnown(entity.kind) || _destroyed.contains(entity.id)) {
            continue;
        }
        _entities.insert_or_assign(entity.id, entity);
        _seen.insert_or_assign(entity.id, part.tick);
    }
    return true;
}

std::vector<RemovedEntity> RemoteWorld::forgetStale()
{
    std::vector<RemovedEntity> removed;
    for (auto seen = _seen.begin(); seen != _seen.end();) {
        if (seen->second + StaleTicks >= _tick) {
            ++seen;
            continue;
        }
        if (const auto entity = _entities.find(seen->first); entity != _entities.end()) {
            removed.push_back({entity->second, DestroyCause::Removed});
            _entities.erase(entity);
        }
        seen = _seen.erase(seen);
    }
    return removed;
}

std::vector<RemovedEntity> RemoteWorld::apply(const EntitiesDestroyedMessage &message)
{
    std::vector<RemovedEntity> removed;
    for (const DestroyedEntity &destroyed : message.entities) {
        // Remembered even if unknown: its first snapshot may still be on its way
        if (!_received || message.tick > _tick) {
            _destroyed.insert_or_assign(destroyed.id, message.tick);
        }
        const auto entity = _entities.find(destroyed.id);
        if (entity != _entities.end()) {
            removed.push_back({entity->second, destroyed.cause});
            _entities.erase(entity);
            _seen.erase(destroyed.id);
        }
    }
    return removed;
}

const EntityState *RemoteWorld::find(std::uint32_t id) const
{
    const auto entity = _entities.find(id);
    return entity == _entities.end() ? nullptr : &entity->second;
}

} // namespace rtype::game
