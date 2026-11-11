#include "rtype/game/Ship/ShipRegistry.hpp"

#include <format>
#include <iterator>
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

std::size_t ShipRegistry::getDefinitionCount() const
{
    return _definitions.size();
}

std::size_t ShipRegistry::getIndex(const std::string &id) const
{
    const auto found = _definitions.find(id);
    if (found == _definitions.end()) {
        throw std::out_of_range(std::format("unknown ship '{}'", id));
    }
    return static_cast<std::size_t>(std::distance(_definitions.begin(), found));
}

const ShipDefinition &ShipRegistry::getDefinitionAt(std::size_t index) const
{
    if (index >= _definitions.size()) {
        throw std::out_of_range(std::format("no ship at index {}", index));
    }
    return std::next(_definitions.begin(), static_cast<std::ptrdiff_t>(index))->second;
}

} // namespace rtype::game
