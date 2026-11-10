#include "Settings/AuraSettings.hpp"

#include <algorithm>
#include <string>

namespace rtype::client
{

namespace
{

/// The name of the choice kept under this key, or the default if there is none or it is not one.
std::string_view readChoice(const engine::Settings &settings, std::string_view key, std::string_view fallback)
{
    const std::optional<std::string> kept = settings.getString(key);
    if (!kept) {
        return fallback;
    }
    const auto found = std::ranges::find(AuraColorChoices, *kept, &AuraColorChoice::name);
    return found != AuraColorChoices.end() ? found->name : fallback;
}

} // namespace

std::optional<render::Color> auraColor(std::string_view name)
{
    const auto found = std::ranges::find(AuraColorChoices, name, &AuraColorChoice::name);
    return found != AuraColorChoices.end() ? found->color : std::nullopt;
}

AuraColors readAuraColors(const engine::Settings &settings)
{
    const AuraColors defaults;
    return {readChoice(settings, OwnAuraSettingsKey, defaults.own),
            readChoice(settings, AllyAuraSettingsKey, defaults.allies),
            readChoice(settings, EnemyAuraSettingsKey, defaults.enemies)};
}

void keepAuraColors(engine::Settings &settings, const AuraColors &colors)
{
    settings.setString(OwnAuraSettingsKey, colors.own);
    settings.setString(AllyAuraSettingsKey, colors.allies);
    settings.setString(EnemyAuraSettingsKey, colors.enemies);
}

} // namespace rtype::client
