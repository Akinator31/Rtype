#pragma once

#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/render/ResourceManager.hpp"

namespace rtype::client
{

/// Name of the loading screen scene.
inline constexpr const char *LoadingScene = "loading";

/// Fills the loading screen: on black, the logo in the middle of the screen, where the main menu draws it,
/// and a bar under it that fills as the assets load. Both are transparent at first: the LoadingSystem fades
/// them and fills the bar. Escape must not close the scene.
/// @param resources Where the logo is loaded from.
/// @param style Look of the bar. It must outlive the scene.
void buildLoadingScene(engine::Engine &engine, engine::SceneId scene, render::ResourceManager &resources,
                       const engine::InterfaceStyle &style, const localization::Localization &translations);

} // namespace rtype::client
