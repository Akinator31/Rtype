#pragma once

#include "rtype/game/Item/ItemDefinition.hpp"

#include <cstddef>
#include <map>
#include <string>

namespace rtype::game
{

/// The kinds of item of a game, by id. Shared by the systems through the GameContext.
///
/// Each item also has an index, its rank among the ids in alphabetical order: the snapshots send it so that
/// the client, which reads the same configuration, draws the model of the item (EntityState::ship).
class ItemRegistry
{
    std::map<std::string, ItemDefinition> _definitions;

public:
    /// Adds a definition. Throws std::invalid_argument if its id is empty or already taken, if it has no
    /// model or no effect, if its lifetime is not above 0 or its speed negative, or if an effect lacks its
    /// numbers: an amount for Heal, ExtraLife and Damage, a duration for RapidFire and Shield, a factor above
    /// 1 for RapidFire.
    void addDefinition(ItemDefinition definition);

    [[nodiscard]] bool hasDefinition(const std::string &id) const;

    /// Throws std::out_of_range if the id is unknown.
    [[nodiscard]] const ItemDefinition &getDefinition(const std::string &id) const;

    [[nodiscard]] std::size_t getDefinitionCount() const;

    /// The index of an item, from 0 to getDefinitionCount() - 1. Throws std::out_of_range if the id is
    /// unknown.
    [[nodiscard]] std::size_t getIndex(const std::string &id) const;

    /// The item at an index. Throws std::out_of_range if the index is not below getDefinitionCount().
    [[nodiscard]] const ItemDefinition &getDefinitionAt(std::size_t index) const;
};

} // namespace rtype::game
