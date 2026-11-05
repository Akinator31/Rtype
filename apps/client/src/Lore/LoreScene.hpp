#pragma once

#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/render/ResourceManager.hpp"

namespace rtype::client
{

/// Name of the scene that tells the story of the game, opened from the Play menu.
inline constexpr const char *LoreScene = "lore";

/// Declares the scene of the story: the lore file of the language of the player scrolls away on the stars, in
/// white, from the bottom of the screen towards the horizon (see Crawl). The scene closes once the text has
/// faded out, or with its back button or Escape. Its music, loaded before it opens, plays once from its start
/// and stops when the scene closes, as it is unloaded.
/// @param resources, audio, style, metrics, translations They must outlive the engine.
void createLoreScene(engine::Engine &engine, render::ResourceManager &resources, audio::AudioPlayer &audio,
                     const engine::InterfaceStyle &style, const engine::Metrics &metrics,
                     const localization::Localization &translations);

} // namespace rtype::client
