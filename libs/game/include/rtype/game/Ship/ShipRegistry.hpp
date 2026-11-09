#pragma once

#include "rtype/game/Ship/ShipDefinition.hpp"

#include <cstddef>
#include <map>
#include <string>

namespace rtype::game
{

/// The kinds of ship of a game, by id. Shared by the systems through the GameContext.
///
/// Each ship also has an index, its rank among the ids in alphabetical order: the snapshots send it so that the
/// client, which reads the same configuration, draws the model of the ship (EntityState::ship).
class ShipRegistry
{
    std::map<std::string, ShipDefinition> _definitions;

public:
    /// Adds a definition. Throws std::invalid_argument if its id is empty or already taken, or if it has no
    /// model.
    void addDefinition(ShipDefinition definition);

    [[nodiscard]] bool hasDefinition(const std::string &id) const;

    /// Throws std::out_of_range if the id is unknown.
    [[nodiscard]] const ShipDefinition &getDefinition(const std::string &id) const;

    [[nodiscard]] std::size_t getDefinitionCount() const;

    /// The index of a ship, from 0 to getDefinitionCount() - 1. Throws std::out_of_range if the id is unknown.
    [[nodiscard]] std::size_t getIndex(const std::string &id) const;

    /// The ship at an index. Throws std::out_of_range if the index is not below getDefinitionCount().
    [[nodiscard]] const ShipDefinition &getDefinitionAt(std::size_t index) const;
};

} // namespace rtype::game
