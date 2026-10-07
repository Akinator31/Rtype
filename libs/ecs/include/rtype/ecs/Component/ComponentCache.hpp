#pragma once

#include "rtype/ecs/Component/ICache.hpp"
#include "rtype/ecs/Entity/Entity.hpp"

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace rtype::ecs
{

/// Components of one type. The index is the entity id, so reaching the component of an entity is a
/// direct memory access. A slot is empty when the entity has no such component.
template <typename T>
class ComponentCache final : public ICache
{
    std::vector<std::optional<T>> _slots;

public:
    /// Attaches a component to an entity, replacing the one it already has.
    /// May reallocate the cache: references to components of this type are invalidated.
    template <typename... Args>
    T &emplace(entity_t entity, Args &&...args)
    {
        if (entity >= _slots.size()) {
            _slots.resize(static_cast<std::size_t>(entity) + 1);
        }
        return _slots[entity].emplace(std::forward<Args>(args)...);
    }

    void erase(entity_t entity) override
    {
        if (entity < _slots.size()) {
            _slots[entity].reset();
        }
    }

    [[nodiscard]] bool has(entity_t entity) const
    {
        return entity < _slots.size() && _slots[entity].has_value();
    }

    /// The entity must have the component (see has()).
    [[nodiscard]] T &get(entity_t entity) { return *_slots[entity]; }
    [[nodiscard]] const T &get(entity_t entity) const { return *_slots[entity]; }

    /// Number of slots, i.e. one past the highest entity id that ever had this component.
    [[nodiscard]] std::size_t size() const { return _slots.size(); }
};

} // namespace rtype::ecs
