#pragma once

#include "rtype/discord/Authorizer.hpp"

#include <cstdint>
#include <memory>

namespace rtype::discord
{

/// The Authorizer of the game, through the Discord Social SDK (libs/discord/sdk): `Client::Authorize` with a
/// challenge of PKCE, which opens the Discord client if it runs, the browser otherwise, and receives the code
/// on a server of its own on the loopback. The SDK starts at the first authorization only: a player who never
/// signs in with Discord never runs it. Its logs go to the logger `discord`, warnings and errors only.
class SocialSdkAuthorizer final : public Authorizer
{
    struct State;
    std::unique_ptr<State> _state;

public:
    SocialSdkAuthorizer();
    ~SocialSdkAuthorizer() override;

    SocialSdkAuthorizer(const SocialSdkAuthorizer &) = delete;
    SocialSdkAuthorizer &operator=(const SocialSdkAuthorizer &) = delete;

    void authorize(std::uint64_t applicationId, Done done) override;
    void cancel() override;
    void poll() override;
};

} // namespace rtype::discord
