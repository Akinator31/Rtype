#pragma once

#include "rtype/discord/Authorizer.hpp"

#include <memory>

namespace rtype::discord
{

class Session;

/// The Authorizer of the game, through the Discord Social SDK (libs/discord/sdk): `Client::Authorize` with a
/// challenge of PKCE, which opens the Discord client if it runs, the browser otherwise, and receives the code
/// on a server of its own on the loopback. The client of the SDK is the one of the Session, started at the
/// first authorization only: a player who never signs in with Discord never runs it.
class SocialSdkAuthorizer final : public Authorizer
{
    struct State;
    std::unique_ptr<State> _state;

public:
    /// Asks Discord through the client of `session`, which outlives the authorizer.
    explicit SocialSdkAuthorizer(Session &session);
    ~SocialSdkAuthorizer() override;

    SocialSdkAuthorizer(const SocialSdkAuthorizer &) = delete;
    SocialSdkAuthorizer &operator=(const SocialSdkAuthorizer &) = delete;

    void authorize(std::uint64_t applicationId, Done done) override;
    void cancel() override;
};

} // namespace rtype::discord
