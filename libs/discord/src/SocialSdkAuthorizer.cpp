#include "rtype/discord/SocialSdkAuthorizer.hpp"

#include "rtype/log/Logger.hpp"

// The C++ wrappers of the SDK are inline in its header, compiled once: here
#define DISCORDPP_IMPLEMENTATION
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
    std::shared_ptr<discordpp::Client> client;
    /// The authorization running counts: what comes back for an older one is dropped.
    std::uint64_t attempt = 0;
    bool running = false;
};

SocialSdkAuthorizer::SocialSdkAuthorizer() : _state(std::make_unique<State>()) {}

SocialSdkAuthorizer::~SocialSdkAuthorizer()
{
    cancel();
    _state->client.reset();
}

void SocialSdkAuthorizer::authorize(std::uint64_t applicationId, Done done)
{
    cancel();
    State &state = *_state;
    if (!state.client) {
        state.client = std::make_shared<discordpp::Client>();
        state.client->AddLogCallback(
            [](std::string message, discordpp::LoggingSeverity severity) {
                // The SDK logs no secret at these levels: neither the code nor the verifier
                while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
                    message.pop_back();
                }
                if (severity == discordpp::LoggingSeverity::Error) {
                    logger().error("{}", message);
                } else {
                    logger().warning("{}", message);
                }
            },
            discordpp::LoggingSeverity::Warning);
    }
    const std::uint64_t attempt = ++state.attempt;
    state.running = true;
    discordpp::AuthorizationCodeVerifier verifier = state.client->CreateAuthorizationCodeVerifier();
    discordpp::AuthorizationArgs args;
    args.SetClientId(applicationId);
    // The master needs to know who the player is, nothing more
    args.SetScopes("identify");
    args.SetCodeChallenge(verifier.Challenge());
    logger().info("Asking Discord for application {}", applicationId);
    state.client->Authorize(args, [this, attempt, codeVerifier = verifier.Verifier(),
                                   done = std::move(done)](discordpp::ClientResult result, std::string code,
                                                           std::string redirectUri) {
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
    if (state.running && state.client) {
        state.running = false;
        state.client->AbortAuthorize();
    }
}

void SocialSdkAuthorizer::poll()
{
    if (_state->client) {
        discordpp::RunCallbacks();
    }
}

} // namespace rtype::discord
