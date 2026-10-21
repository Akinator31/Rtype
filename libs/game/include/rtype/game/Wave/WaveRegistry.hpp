#pragma once

#include "rtype/game/Enemy/EnemyRegistry.hpp"
#include "rtype/game/Wave/WaveDefinition.hpp"

#include <string>
#include <unordered_map>

namespace rtype::game
{

/// The waves of a game, by name. Shared by the systems through the GameContext.
class WaveRegistry
{
    std::unordered_map<std::string, WaveDefinition> _definitions;

public:
    /// Adds a wave. Throws std::invalid_argument if its name is empty or already taken, if its quantity is 0,
    /// its interval negative, or its placement incomplete: a Manual wave needs `spawns` with `quantity`
    /// enemies and no `enemy`, a Random wave needs an `enemy`, no `spawns` and an area with min <= max. Every
    /// enemy it names must already be in `enemies` (the enemies are filled first).
    void addDefinition(WaveDefinition definition, const EnemyRegistry &enemies);

    [[nodiscard]] bool hasDefinition(const std::string &name) const;

    /// Throws std::out_of_range if the name is unknown.
    [[nodiscard]] const WaveDefinition &getDefinition(const std::string &name) const;
};

} // namespace rtype::game
