#include "rtype/game/Projectile/ProjectileRegistry.hpp"

#include <format>
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

} // namespace rtype::game
