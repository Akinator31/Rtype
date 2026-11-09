#pragma once

#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/render/ResourceManager.hpp"

#include <functional>

namespace rtype::client
{

/// Name of the scene that opens the campaign, from the Play menu.
inline constexpr const char *CampaignScene = "campaign";

/// Declares the opening of the campaign: on a black screen, the briefing of the mission of the language of
/// the player is received as a radio transmission (see Transmission), over the hum of the open channel. Its
/// first line, "# " in the file, is the name of the mission; the lines before the first empty line are the
/// facts of the mission, those after it the message, cut into lines as wide as the column. Space, Enter or a
/// click skips to its end, then goes on: the scene closes and the campaign starts with the ship
/// equipped, without the hangar. Escape closes it.
/// @param resources, audio, metrics, translations They must outlive the engine.
/// @param start Starts the campaign, once the scene has closed.
void createCampaignScene(engine::Engine &engine, render::ResourceManager &resources,
                         audio::AudioPlayer &audio, const engine::Metrics &metrics,
                         const localization::Localization &translations, std::function<void()> start);

} // namespace rtype::client
