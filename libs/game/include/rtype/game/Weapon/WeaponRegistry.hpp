#pragma once

#include "rtype/game/Weapon/WeaponDefinition.hpp"

#include <string>
#include <unordered_map>

namespace rtype::game
{

/// The kinds of weapon of a game, by id. Shared by the systems through the GameContext.
class WeaponRegistry
{
    std::unordered_map<std::string, WeaponDefinition> _definitions;

public:
    /// Adds a definition. Throws std::invalid_argument if its id is empty or already taken, or if its
    /// fireInterval or its damage is 0.
    void addDefinition(WeaponDefinition definition);

    [[nodiscard]] bool hasDefinition(const std::string &id) const;

    /// Throws std::out_of_range if the id is unknown.
    [[nodiscard]] const WeaponDefinition &getDefinition(const std::string &id) const;
};

} // namespace rtype::game
