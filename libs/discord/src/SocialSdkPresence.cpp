#include "rtype/discord/SocialSdkPresence.hpp"

#include "rtype/discord/Session.hpp"
#include "rtype/log/Logger.hpp"

#include <discordpp.h>

#include <chrono>
#include <optional>
#include <set>
#include <string>
#include <utility>

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
        party.SetId(activity.party->id.empty() ? "game" : activity.party->id);
        party.SetCurrentSize(activity.party->size);
        party.SetMaxSize(activity.party->max);
        // Joinable, the friends of the player see Join on its profile: the games are listed to all anyway
        party.SetPrivacy(activity.joinSecret.empty() ? discordpp::ActivityPartyPrivacy::Private
                                                     : discordpp::ActivityPartyPrivacy::Public);
        result.SetParty(party);
    }
    if (!activity.joinSecret.empty()) {
        discordpp::ActivitySecrets secrets;
        secrets.SetJoin(activity.joinSecret);
        result.SetSecrets(secrets);
        result.SetSupportedPlatforms(discordpp::ActivityGamePlatforms::Desktop);
    }
    if (activity.startedAt) {
        discordpp::ActivityTimestamps timestamps;
        const auto since =
            std::chrono::duration_cast<std::chrono::milliseconds>(activity.startedAt->time_since_epoch());
        timestamps.SetStart(static_cast<std::uint64_t>(since.count()));
        result.SetTimestamps(timestamps);
    }
    if (!activity.largeImage.empty() || !activity.smallImage.empty()) {
        discordpp::ActivityAssets assets;
        assets.SetLargeImage(textOf(activity.largeImage));
        assets.SetLargeText(textOf(activity.largeText));
        assets.SetSmallImage(textOf(activity.smallImage));
        assets.SetSmallText(textOf(activity.smallText));
        result.SetAssets(assets);
    }
    return result;
}

/// Leaves out of `activity` the image Discord could not find on the page of the application ("Unable to
/// resolve large image asset: vx-01"), and gives its key; none if the error is not that.
std::optional<std::string> dropMissingImage(Activity &activity, const discordpp::ClientResult &result)
{
    const std::string error = result.Error();
    if (error.find("asset") == std::string::npos) {
        return std::nullopt;
    }
    const bool small = error.find("small") != std::string::npos;
    std::string &image = small ? activity.smallImage : activity.largeImage;
    if (image.empty()) {
        return std::nullopt;
    }
    std::string key = std::exchange(image, std::string());
    (small ? activity.smallText : activity.largeText).clear();
    return key;
}

} // namespace

struct SocialSdkPresence::State {
    /// The last update was refused: the next refusals are not logged.
    bool refused = false;
    /// The images Discord did not find: left out of the next activities.
    std::set<std::string> missingImages;
};

namespace
{

void send(Session &session, const std::weak_ptr<SocialSdkPresence::State> &state, Activity activity,
          Presence::Shown shown);

} // namespace

SocialSdkPresence::SocialSdkPresence(Session &session, std::uint64_t applicationId)
    : _session(session), _applicationId(applicationId), _state(std::make_shared<State>())
{
}

SocialSdkPresence::~SocialSdkPresence()
{
    clear();
}

void SocialSdkPresence::update(const Activity &activity, Shown shown)
{
    if (_applicationId == 0) {
        return;
    }
    start();
    _shown = true;
    Activity sent = activity;
    if (_state->missingImages.contains(sent.largeImage)) {
        sent.largeImage.clear();
        sent.largeText.clear();
    }
    if (_state->missingImages.contains(sent.smallImage)) {
        sent.smallImage.clear();
        sent.smallText.clear();
    }
    send(_session, _state, std::move(sent), std::move(shown));
}

void SocialSdkPresence::start()
{
    if (_started) {
        return;
    }
    _started = true;
    _session.client().SetApplicationId(_applicationId);
    logger().info("Showing the activity of the player in Discord, application {}", _applicationId);
}

void SocialSdkPresence::onJoin(JoinAccepted joined)
{
    if (_applicationId == 0) {
        return;
    }
    start();
    discordpp::Client &client = _session.client();
    // Discord launches the executable running, to join a friend while the game is closed
    if (!client.RegisterLaunchCommand(_applicationId, "")) {
        logger().warning("Discord cannot launch the game to join a friend");
    }
    client.SetActivityJoinCallback(
        [state = std::weak_ptr<State>(_state), joined = std::move(joined)](const std::string &secret) {
            if (state.lock() && joined) {
                // The secret is never logged: it leads into the game of a friend
                logger().info("The player joins a friend from Discord");
                joined(secret);
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

namespace
{

void send(Session &session, const std::weak_ptr<SocialSdkPresence::State> &state, Activity activity,
          Presence::Shown shown)
{
    discordpp::Activity described = activityOf(activity);
    session.client().UpdateRichPresence(
        std::move(described), [&session, state, activity = std::move(activity),
                               shown = std::move(shown)](const discordpp::ClientResult &result) mutable {
            const bool successful = result.Successful();
            const std::shared_ptr<SocialSdkPresence::State> alive = state.lock();
            if (!alive) {
                return;
            }
            if (!successful) {
                if (std::optional<std::string> missing = dropMissingImage(activity, result)) {
                    // Not uploaded on the page of the application: shown without it rather than not at all
                    logger().warning("Discord has no image \"{}\" for the activity: upload it on the Rich "
                                     "Presence page of the application",
                                     *missing);
                    alive->missingImages.insert(std::move(*missing));
                    send(session, state, std::move(activity), std::move(shown));
                    return;
                }
            }
            // Discord not running, most often: nothing is shown, never an error for the player. Logged once,
            // the caller sending it again until Discord takes it
            if (!successful && !std::exchange(alive->refused, true)) {
                logger().info("Discord shows no activity: {}", result.Error());
            } else if (successful && std::exchange(alive->refused, false)) {
                logger().info("Discord shows the activity again");
            }
            if (shown) {
                shown(successful);
            }
        });
}

} // namespace

} // namespace rtype::discord
