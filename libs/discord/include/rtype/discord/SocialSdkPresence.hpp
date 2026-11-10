#pragma once

#include "rtype/discord/Presence.hpp"

#include <cstdint>

namespace rtype::discord
{

class Session;

/// The Presence of the game, through the Discord Social SDK: `Client::UpdateRichPresence` without a token,
/// which the Discord client of the player shows when it runs (IPC). The activity is the one of an application
/// of Discord, its id given here: the official one of R-Type. Its first update starts the client of the
/// Session; without Discord running, the SDK answers an error, logged, and nothing is shown.
///
/// Discord limits the updates of a presence (5 in 20 s): the caller sends only the changes, spaced
/// (client::PresenceTracker).
class SocialSdkPresence final : public Presence
{
    Session &_session;
    std::uint64_t _applicationId;
    /// The application was given to the client of the SDK.
    bool _started = false;
    /// An activity is shown: clear() has something to clear.
    bool _shown = false;

public:
    /// Shows the activity through the client of `session`, which outlives it, as the one of `applicationId`.
    /// An id 0 shows nothing.
    SocialSdkPresence(Session &session, std::uint64_t applicationId);
    ~SocialSdkPresence() override;

    SocialSdkPresence(const SocialSdkPresence &) = delete;
    SocialSdkPresence &operator=(const SocialSdkPresence &) = delete;

    void update(const Activity &activity) override;
    void clear() override;
};

} // namespace rtype::discord
