#include "rtype/discord/SocialSdkAuthorizer.hpp"

#include "rtype/discord/Session.hpp"
#include "rtype/log/Logger.hpp"

#include <discordpp.h>

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

} // namespace

struct SocialSdkAuthorizer::State {
    Session &session;
    /// The authorization running counts: what comes back for an older one is dropped.
    std::uint64_t attempt = 0;
    bool running = false;
};

SocialSdkAuthorizer::SocialSdkAuthorizer(Session &session)
    : _state(std::make_unique<State>(State{.session = session}))
{
}

SocialSdkAuthorizer::~SocialSdkAuthorizer()
{
    cancel();
}

void SocialSdkAuthorizer::authorize(std::uint64_t applicationId, const std::string &scopes, Done done)
{
    cancel();
    State &state = *_state;
    discordpp::Client &client = state.session.client();
    const std::uint64_t attempt = ++state.attempt;
    state.running = true;
    discordpp::AuthorizationCodeVerifier verifier = client.CreateAuthorizationCodeVerifier();
    discordpp::AuthorizationArgs args;
    args.SetClientId(applicationId);
    // What the master asks: who the player is, and the social features when it gives the token
    args.SetScopes(scopes);
    args.SetCodeChallenge(verifier.Challenge());
    logger().info("Asking Discord for application {}, scopes {}", applicationId, scopes);
    client.Authorize(args, [this, attempt, codeVerifier = verifier.Verifier(), done = std::move(done)](
                               discordpp::ClientResult result, std::string code, std::string redirectUri) {
        if (attempt != _state->attempt || !_state->running) {
            return;
        }
        _state->running = false;
        if (!result.Successful()) {
            const bool refused = result.Type() == discordpp::ErrorType::AuthorizationFailed ||
                                 result.Type() == discordpp::ErrorType::Aborted;
            logger().info("Discord gave no authorization: {}", result.Error());
            done(refused ? AuthorizationError::Refused : AuthorizationError::Failed);
            return;
        }
        logger().info("Discord authorized the application");
        done(Authorization{
            .code = std::move(code), .codeVerifier = codeVerifier, .redirectUri = std::move(redirectUri)});
    });
}

void SocialSdkAuthorizer::cancel()
{
    State &state = *_state;
    if (state.running) {
        state.running = false;
        state.session.client().AbortAuthorize();
    }
}

} // namespace rtype::discord
