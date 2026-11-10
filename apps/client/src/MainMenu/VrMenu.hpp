#pragma once

#include "Settings/VrSettings.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/localization/Localization.hpp"

#include <string>

namespace rtype::client
{

/// Name of the VR page, opened from the Settings page.
inline constexpr const char *VrMenuScene = "vr_menu";

/// Declares the VR page (docs/architecture/vr.md#the-vr-page): on the left, the headset (VR on or off, which
/// opens or closes it at once, the recentre, the snap turn, what the window shows); on the right, the board
/// (in relief or flat, its distance, its size) and the height of the seat. Each choice applies at once; they
/// are written to the settings of the engine when the page closes (see VrSettings.hpp).
/// @param style Look of the interface. It must outlive the engine, as the translations and the targets.
void createVrScene(engine::Engine &engine, const engine::InterfaceStyle &style,
                   const localization::Localization &translations, const VrTargets &targets);

/// What a row of the VR page shows for a length in metres: "2.7 m".
[[nodiscard]] std::string metresText(float metres);
/// What a row of the VR page shows for a height in centimetres, with its sign: "+5 cm", "0 cm", "-10 cm".
[[nodiscard]] std::string centimetresText(float metres);

} // namespace rtype::client
