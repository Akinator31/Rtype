#include "rtype/game/Enemy/EnemyRegistry.hpp"

#include <format>
#include <stdexcept>
#include <utility>

namespace rtype::game
{

namespace
{

void checkDefinition(const EnemyDefinition &definition)
{
    if (definition.id.empty()) {
        throw std::invalid_argument("an enemy needs an id");
    }
    if (definition.ship.empty()) {
        throw std::invalid_argument(std::format("the enemy '{}' needs a ship", definition.id));
    }
    if (definition.health == 0) {
        throw std::invalid_argument(std::format("the enemy '{}' needs health", definition.id));
    }
    if (!(definition.speed > 0.0f)) {
        throw std::invalid_argument(std::format("the enemy '{}' needs a speed above 0", definition.id));
    }
    if (!(definition.acceleration >= 0.0f)) {
        throw std::invalid_argument(
            std::format("the enemy '{}' needs a positive acceleration", definition.id));
    }
    if (definition.weapon.empty()) {
        throw std::invalid_argument(std::format("the enemy '{}' needs a weapon", definition.id));
    }
}

} // namespace

void EnemyRegistry::addDefinition(EnemyDefinition definition)
{
    checkDefinition(definition);
    if (_definitions.contains(definition.id)) {
        throw std::invalid_argument(std::format("the enemy '{}' is already defined", definition.id));
    }
    const std::string id = definition.id;
    _definitions.emplace(id, std::move(definition));
}

bool EnemyRegistry::hasDefinition(const std::string &id) const
{
    return _definitions.contains(id);
}

const EnemyDefinition &EnemyRegistry::getDefinition(const std::string &id) const
{
    const auto found = _definitions.find(id);
    if (found == _definitions.end()) {
        throw std::out_of_range(std::format("unknown enemy '{}'", id));
    }
    return found->second;
}

} // namespace rtype::game
