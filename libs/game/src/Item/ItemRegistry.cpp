#include "rtype/game/Item/ItemRegistry.hpp"

#include <format>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace rtype::game
{

namespace
{

void checkEffect(const ItemDefinition &definition, const ItemEffect &effect)
{
    switch (effect.type) {
    case ItemEffectType::Heal:
    case ItemEffectType::ExtraLife:
    case ItemEffectType::Damage:
        if (effect.amount == 0) {
            throw std::invalid_argument(
                std::format("an effect of the item '{}' needs an amount above 0", definition.id));
        }
        return;
    case ItemEffectType::RapidFire:
        if (!(effect.factor > 1.0f)) {
            throw std::invalid_argument(
                std::format("the rapid fire of the item '{}' needs a factor above 1", definition.id));
        }
        [[fallthrough]];
    case ItemEffectType::Shield:
        if (!(effect.duration > 0.0f)) {
            throw std::invalid_argument(
                std::format("an effect of the item '{}' needs a duration above 0", definition.id));
        }
        return;
    }
}

void checkDefinition(const ItemDefinition &definition)
{
    if (definition.id.empty()) {
        throw std::invalid_argument("an item needs an id");
    }
    if (definition.model.empty()) {
        throw std::invalid_argument(std::format("the item '{}' needs a model", definition.id));
    }
    if (definition.effects.empty()) {
        throw std::invalid_argument(std::format("the item '{}' needs an effect", definition.id));
    }
    if (!(definition.lifetime > 0.0f)) {
        throw std::invalid_argument(std::format("the item '{}' needs a lifetime above 0", definition.id));
    }
    if (!(definition.speed >= 0.0f)) {
        throw std::invalid_argument(std::format("the item '{}' needs a speed of 0 or more", definition.id));
    }
    for (const ItemEffect &effect : definition.effects) {
        checkEffect(definition, effect);
    }
}

} // namespace

void ItemRegistry::addDefinition(ItemDefinition definition)
{
    checkDefinition(definition);
    if (_definitions.contains(definition.id)) {
        throw std::invalid_argument(std::format("the item '{}' is already defined", definition.id));
    }
    const std::string id = definition.id;
    _definitions.emplace(id, std::move(definition));
}

bool ItemRegistry::hasDefinition(const std::string &id) const
{
    return _definitions.contains(id);
}

const ItemDefinition &ItemRegistry::getDefinition(const std::string &id) const
{
    const auto found = _definitions.find(id);
    if (found == _definitions.end()) {
        throw std::out_of_range(std::format("unknown item '{}'", id));
    }
    return found->second;
}

std::size_t ItemRegistry::getDefinitionCount() const
{
    return _definitions.size();
}

std::size_t ItemRegistry::getIndex(const std::string &id) const
{
    const auto found = _definitions.find(id);
    if (found == _definitions.end()) {
        throw std::out_of_range(std::format("unknown item '{}'", id));
    }
    return static_cast<std::size_t>(std::distance(_definitions.begin(), found));
}

const ItemDefinition &ItemRegistry::getDefinitionAt(std::size_t index) const
{
    if (index >= _definitions.size()) {
        throw std::out_of_range(std::format("no item at index {}", index));
    }
    return std::next(_definitions.begin(), static_cast<std::ptrdiff_t>(index))->second;
}

} // namespace rtype::game
