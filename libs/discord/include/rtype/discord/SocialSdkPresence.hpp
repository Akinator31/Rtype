#pragma once

#include "rtype/discord/Presence.hpp"

#include <cstdint>
#include <memory>

namespace rtype::discord
{

class Session;

/// The Presence of the game, through the Discord Social SDK: `Client::UpdateRichPresence` without a token,
/// which the Discord client of the player shows when it runs (IPC). The activity is the one of an application
/// of Discord, its id given here: the official one of R-Type. Its first update starts the client of the
/// Session; without Discord running, the SDK answers an error, logged once until an activity is shown again,
/// and nothing is shown. Connected with a token, Discord refuses an activity whose image is not uploaded on
/// the *Rich Presence* page of the application: it is sent again at once without its image, and that image is
/// left out until the game closes.
///
/// Discord limits the updates of a presence (5 in 20 s): the caller sends only the changes, spaced
/// (client::PresenceTracker).
class SocialSdkPresence final : public Presence
{
public:
    /// What the callbacks of the SDK reach: defined in the implementation.
    struct State;

private:
    Session &_session;
    std::uint64_t _applicationId;
    /// The application was given to the client of the SDK.
    bool _started = false;
    /// An activity is shown: clear() has something to clear.
    bool _shown = false;
    /// Gives the application to the client of the SDK, once.
    void start();

    /// Shared with the callbacks of the SDK, which may run after the presence is gone.
    std::shared_ptr<State> _state;

public:
    /// Shows the activity through the client of `session`, which outlives it, as the one of `applicationId`.
    /// An id 0 shows nothing.
    SocialSdkPresence(Session &session, std::uint64_t applicationId);
    ~SocialSdkPresence() override;

    SocialSdkPresence(const SocialSdkPresence &) = delete;
    SocialSdkPresence &operator=(const SocialSdkPresence &) = delete;

    void update(const Activity &activity, Shown shown) override;
    void clear() override;
    void onJoin(JoinAccepted joined) override;
};

} // namespace rtype::discord
