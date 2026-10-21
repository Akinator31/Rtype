#pragma once

#include "rtype/game/Enemy/EnemyDefinition.hpp"

#include <string>
#include <unordered_map>

namespace rtype::game
{

/// The kinds of enemy of a game, by id. Shared by the systems through the GameContext.
class EnemyRegistry
{
    std::unordered_map<std::string, EnemyDefinition> _definitions;

public:
    /// Adds a definition. Throws std::invalid_argument if its id is empty or already taken, if it has no
    /// ship, no health, no weapon, a speed that is not above 0 or a negative acceleration. The ship and the
    /// weapon are not looked up here, so that the registries can be filled in any order.
    void addDefinition(EnemyDefinition definition);

    [[nodiscard]] bool hasDefinition(const std::string &id) const;

    /// Throws std::out_of_range if the id is unknown.
    [[nodiscard]] const EnemyDefinition &getDefinition(const std::string &id) const;
};

} // namespace rtype::game
