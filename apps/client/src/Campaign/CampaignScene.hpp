#pragma once

#include "Campaign/CampaignMission.hpp"
#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/render/ResourceManager.hpp"

namespace rtype::client
{

/// Name of the scene that opens the campaign, from the Play menu.
inline constexpr const char *CampaignScene = "campaign";

/// Declares the briefing of a mission of the campaign (Campaign::brief()): on a black screen, the briefing of
/// the mission in the language of the player is received as a radio transmission (see Transmission), over the
/// hum of the open channel. Its first line, "# " in the file, is the name of the mission; the lines before
/// the first empty line are the facts of the mission, those after it the message, cut into lines as wide as
/// the column. Space, Enter or a click skips to its end, then goes on: the mission is played with the ship
/// equipped, without the hangar (Campaign::play()). Escape leaves it (Campaign::leave()).
/// @param resources, audio, metrics, translations, campaign They must outlive the engine.
void createCampaignScene(engine::Engine &engine, render::ResourceManager &resources,
                         audio::AudioPlayer &audio, const engine::Metrics &metrics,
                         const localization::Localization &translations, Campaign &campaign);

} // namespace rtype::client
