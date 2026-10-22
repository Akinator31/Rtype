#pragma once

#include "rtype/game/Enemy/EnemyRegistry.hpp"
#include "rtype/game/Wave/WaveDefinition.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

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

    /// The names of the waves of a difficulty, in alphabetical order, so that the same random number picks
    /// the same wave on every machine. Empty if there is none.
    [[nodiscard]] std::vector<std::string> listWavesOfDifficulty(std::uint8_t difficulty) const;

    [[nodiscard]] bool isEmpty() const { return _definitions.empty(); }
};

} // namespace rtype::game
