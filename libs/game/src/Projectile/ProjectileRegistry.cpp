#include "rtype/game/Projectile/ProjectileRegistry.hpp"

#include <format>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace rtype::game
{

void ProjectileRegistry::addDefinition(ProjectileDefinition definition)
{
    if (definition.id.empty()) {
        throw std::invalid_argument("a projectile needs an id");
    }
    if (_definitions.contains(definition.id)) {
        throw std::invalid_argument(std::format("the projectile '{}' is already defined", definition.id));
    }
    const std::string id = definition.id;
    _definitions.emplace(id, std::move(definition));
}

bool ProjectileRegistry::hasDefinition(const std::string &id) const
{
    return _definitions.contains(id);
}

const ProjectileDefinition &ProjectileRegistry::getDefinition(const std::string &id) const
{
    const auto found = _definitions.find(id);
    if (found == _definitions.end()) {
        throw std::out_of_range(std::format("unknown projectile '{}'", id));
    }
    return found->second;
}

std::size_t ProjectileRegistry::getDefinitionCount() const
{
    return _definitions.size();
}

std::size_t ProjectileRegistry::getIndex(const std::string &id) const
{
    const auto found = _definitions.find(id);
    if (found == _definitions.end()) {
        throw std::out_of_range(std::format("unknown projectile '{}'", id));
    }
    return static_cast<std::size_t>(std::distance(_definitions.begin(), found));
}

const ProjectileDefinition &ProjectileRegistry::getDefinitionAt(std::size_t index) const
{
    if (index >= _definitions.size()) {
        throw std::out_of_range(std::format("no projectile at index {}", index));
    }
    return std::next(_definitions.begin(), static_cast<std::ptrdiff_t>(index))->second;
}

} // namespace rtype::game
