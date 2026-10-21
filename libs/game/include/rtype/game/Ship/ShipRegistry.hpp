#pragma once

#include "rtype/game/Ship/ShipDefinition.hpp"

#include <string>
#include <unordered_map>

namespace rtype::game
{

/// The kinds of ship of a game, by id. Shared by the systems through the GameContext.
class ShipRegistry
{
    std::unordered_map<std::string, ShipDefinition> _definitions;

public:
    /// Adds a definition. Throws std::invalid_argument if its id is empty or already taken, or if it has no
    /// model.
    void addDefinition(ShipDefinition definition);

    [[nodiscard]] bool hasDefinition(const std::string &id) const;

    /// Throws std::out_of_range if the id is unknown.
    [[nodiscard]] const ShipDefinition &getDefinition(const std::string &id) const;
};

} // namespace rtype::game
