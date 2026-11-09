#include "rtype/discord/SocialSdkPresence.hpp"

#include "rtype/discord/Session.hpp"
#include "rtype/log/Logger.hpp"

#include <discordpp.h>

#include <chrono>
#include <string>

namespace rtype::discord
{

namespace
{

const log::Logger &logger()
{
    static const log::Logger instance("discord");
    return instance;
}

/// An empty text is not shown: the SDK takes none.
std::optional<std::string> textOf(const std::string &text)
{
    return text.empty() ? std::nullopt : std::optional<std::string>(text);
}

discordpp::Activity activityOf(const Activity &activity)
{
    discordpp::Activity result;
    result.SetType(discordpp::ActivityTypes::Playing);
    result.SetState(textOf(activity.state));
    result.SetDetails(textOf(activity.details));
    if (activity.party) {
        discordpp::ActivityParty party;
        // Not joinable yet: the id only makes Discord show the size
        party.SetId("game");
        party.SetCurrentSize(activity.party->size);
        party.SetMaxSize(activity.party->max);
        party.SetPrivacy(discordpp::ActivityPartyPrivacy::Private);
        result.SetParty(party);
    }
    if (activity.startedAt) {
        discordpp::ActivityTimestamps timestamps;
        const auto since =
            std::chrono::duration_cast<std::chrono::milliseconds>(activity.startedAt->time_since_epoch());
        timestamps.SetStart(static_cast<std::uint64_t>(since.count()));
        result.SetTimestamps(timestamps);
    }
    if (!activity.largeImage.empty() || !activity.largeText.empty()) {
        discordpp::ActivityAssets assets;
        assets.SetLargeImage(textOf(activity.largeImage));
        assets.SetLargeText(textOf(activity.largeText));
        result.SetAssets(assets);
    }
    return result;
}

} // namespace

SocialSdkPresence::SocialSdkPresence(Session &session, std::uint64_t applicationId)
    : _session(session), _applicationId(applicationId)
{
}

SocialSdkPresence::~SocialSdkPresence()
{
    clear();
}

void SocialSdkPresence::update(const Activity &activity)
{
    if (_applicationId == 0) {
        return;
    }
    discordpp::Client &client = _session.client();
    if (!_started) {
        _started = true;
        client.SetApplicationId(_applicationId);
        logger().info("Showing the activity of the player in Discord, application {}", _applicationId);
    }
    _shown = true;
    client.UpdateRichPresence(activityOf(activity), [](const discordpp::ClientResult &result) {
        if (!result.Successful()) {
            // Discord not running, most often: nothing is shown, never an error for the player
            logger().info("Discord shows no activity: {}", result.Error());
        }
    });
}

void SocialSdkPresence::clear()
{
    if (_shown) {
        _shown = false;
        _session.client().ClearRichPresence();
    }
}

} // namespace rtype::discord
