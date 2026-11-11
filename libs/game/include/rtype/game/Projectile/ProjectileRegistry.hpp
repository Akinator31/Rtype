#pragma once

#include "rtype/game/Projectile/ProjectileDefinition.hpp"

#include <cstddef>
#include <map>
#include <string>

namespace rtype::game
{

/// The kinds of projectile of a game, by id. Shared by the systems through the GameContext.
///
/// Each projectile also has an index, its rank among the ids in alphabetical order: the snapshots send it for
/// the shots of the players, so that the client, which reads the same configuration, draws the model of the
/// projectile (EntityState::ship).
class ProjectileRegistry
{
    std::map<std::string, ProjectileDefinition> _definitions;

public:
    /// Adds a definition. Throws std::invalid_argument if its id is empty or already taken.
    void addDefinition(ProjectileDefinition definition);

    [[nodiscard]] bool hasDefinition(const std::string &id) const;

    /// Throws std::out_of_range if the id is unknown.
    [[nodiscard]] const ProjectileDefinition &getDefinition(const std::string &id) const;

    [[nodiscard]] std::size_t getDefinitionCount() const;

    /// The index of a projectile, from 0 to getDefinitionCount() - 1. Throws std::out_of_range if the id is
    /// unknown.
    [[nodiscard]] std::size_t getIndex(const std::string &id) const;

    /// The projectile at an index. Throws std::out_of_range if the index is not below getDefinitionCount().
    [[nodiscard]] const ProjectileDefinition &getDefinitionAt(std::size_t index) const;
};

} // namespace rtype::game
