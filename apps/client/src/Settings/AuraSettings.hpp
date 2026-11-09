#pragma once

#include "rtype/engine/Settings/Settings.hpp"
#include "rtype/render/Types.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace rtype::client
{

/// Where the colours of the auras chosen on the Settings page are kept in the settings of the engine.
inline constexpr std::string_view OwnAuraSettingsKey = "game.aura.own";
inline constexpr std::string_view AllyAuraSettingsKey = "game.aura.allies";
inline constexpr std::string_view EnemyAuraSettingsKey = "game.aura.enemies";

/// A colour a player can give to an aura, or none: no aura.
struct AuraColorChoice {
    /// How it is written in the settings ("green"), and the end of the key of its name in the translations
    /// ("settings.aura_color.green").
    std::string_view name;
    /// None for no aura.
    std::optional<render::Color> color;
};

/// The colours offered on the Settings page, in the order its rows go through them.
inline constexpr std::array<AuraColorChoice, 9> AuraColorChoices{{
    {"off", std::nullopt},
    {"blue", render::Color{66, 165, 245, 255}},
    {"cyan", render::Color{38, 230, 255, 255}},
    {"green", render::Color{102, 230, 106, 255}},
    {"yellow", render::Color{255, 214, 40, 255}},
    {"orange", render::Color{255, 140, 40, 255}},
    {"red", render::Color{255, 64, 64, 255}},
    {"purple", render::Color{190, 100, 255, 255}},
    {"white", render::Color{240, 240, 240, 255}},
}};

/// The colours of the auras around the ships in the game, by the name of their AuraColorChoice: the ship of
/// the local player, those of the other players and of their allies, and those of the Bydos.
struct AuraColors {
    std::string_view own = "green";
    std::string_view allies = "blue";
    std::string_view enemies = "red";
};

/// The colour of the AuraColorChoice of this name, or nothing for "off" or a name that is not one.
[[nodiscard]] std::optional<render::Color> auraColor(std::string_view name);

/// The colours kept in the settings. A colour missing, or a name that is not one of AuraColorChoices, keeps
/// the default of AuraColors.
[[nodiscard]] AuraColors readAuraColors(const engine::Settings &settings);
/// Keeps the colours in the settings, without writing them.
void keepAuraColors(engine::Settings &settings, const AuraColors &colors);

} // namespace rtype::client
