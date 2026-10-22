#include "rtype/game/Weapon/WeaponRegistry.hpp"

#include <format>
#include <stdexcept>
#include <utility>

namespace rtype::game
{

void WeaponRegistry::addDefinition(WeaponDefinition definition)
{
    if (definition.id.empty()) {
        throw std::invalid_argument("a weapon needs an id");
    }
    if (definition.fireInterval == 0) {
        throw std::invalid_argument(std::format("the weapon '{}' needs a fire interval", definition.id));
    }
    if (definition.damage == 0) {
        throw std::invalid_argument(std::format("the weapon '{}' needs damage", definition.id));
    }
    if (_definitions.contains(definition.id)) {
        throw std::invalid_argument(std::format("the weapon '{}' is already defined", definition.id));
    }
    const std::string id = definition.id;
    _definitions.emplace(id, std::move(definition));
}

bool WeaponRegistry::hasDefinition(const std::string &id) const
{
    return _definitions.contains(id);
}

const WeaponDefinition &WeaponRegistry::getDefinition(const std::string &id) const
{
    const auto found = _definitions.find(id);
    if (found == _definitions.end()) {
        throw std::out_of_range(std::format("unknown weapon '{}'", id));
    }
    return found->second;
}

} // namespace rtype::game
