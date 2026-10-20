#pragma once

#include "rtype/game/Projectile/ProjectileDefinition.hpp"

#include <string>
#include <unordered_map>

namespace rtype::game
{

/// The kinds of projectile of a game, by id. Shared by the systems through the GameContext.
class ProjectileRegistry
{
    std::unordered_map<std::string, ProjectileDefinition> _definitions;

public:
    /// Adds a definition. Throws std::invalid_argument if its id is empty or already taken.
    void addDefinition(ProjectileDefinition definition);

    [[nodiscard]] bool hasDefinition(const std::string &id) const;

    /// Throws std::out_of_range if the id is unknown.
    [[nodiscard]] const ProjectileDefinition &getDefinition(const std::string &id) const;
};

} // namespace rtype::game
