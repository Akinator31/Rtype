#pragma once

#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/localization/Localization.hpp"

namespace rtype::client
{

/// Name of the scene that shows the elements of the interface, opened from the settings.
inline constexpr const char *UiShowcaseScene = "ui_showcase";

/// Declares the scene that shows the elements of the interface in their states, to see them as the player
/// does: the titles, the sliders, which can be dragged, the flare buttons, and one button for each kind of
/// toast.
/// @param style Look of the interface. It must outlive the engine.
void createUiShowcaseScene(engine::Engine &engine, const engine::InterfaceStyle &style,
                           localization::Localization &translations);

} // namespace rtype::client
