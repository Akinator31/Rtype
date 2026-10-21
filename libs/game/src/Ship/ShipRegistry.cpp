#include "rtype/game/Ship/ShipRegistry.hpp"

#include <format>
#include <stdexcept>
#include <utility>

namespace rtype::game
{

void ShipRegistry::addDefinition(ShipDefinition definition)
{
    if (definition.id.empty()) {
        throw std::invalid_argument("a ship needs an id");
    }
    if (definition.model.empty()) {
        throw std::invalid_argument(std::format("the ship '{}' needs a model", definition.id));
    }
    if (_definitions.contains(definition.id)) {
        throw std::invalid_argument(std::format("the ship '{}' is already defined", definition.id));
    }
    const std::string id = definition.id;
    _definitions.emplace(id, std::move(definition));
}

bool ShipRegistry::hasDefinition(const std::string &id) const
{
    return _definitions.contains(id);
}

const ShipDefinition &ShipRegistry::getDefinition(const std::string &id) const
{
    const auto found = _definitions.find(id);
    if (found == _definitions.end()) {
        throw std::out_of_range(std::format("unknown ship '{}'", id));
    }
    return found->second;
}

} // namespace rtype::game
