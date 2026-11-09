#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <variant>

/// Signing in with Discord from the client (docs/architecture/client-online.md#discord): the player accepts
/// the application of a master in Discord, and the client gets a code the master exchanges for who the player
/// is. The client and its tests know only this interface; the Discord Social SDK is behind
/// SocialSdkAuthorizer (rtype/discord/SocialSdkAuthorizer.hpp), linked by the executable of the client alone.
namespace rtype::discord
{

/// What Discord gives once the player accepted, for DISCORD_CONFIRM. Secret: never logged.
struct Authorization {
    /// The authorization code, which only the secret of the application, with the verifier, exchanges.
    std::string code;
    /// The code verifier of PKCE (RFC 7636), whose challenge went to Discord.
    std::string codeVerifier;
    /// Where Discord sent the code back: the master gives it to Discord again with the code.
    std::string redirectUri;

    friend bool operator==(const Authorization &, const Authorization &) = default;
};

/// Why the player gave no authorization.
enum class AuthorizationError {
    /// The player refused, or closed the page.
    Refused,
    /// Discord, or the browser, could not be reached.
    Failed,
};

using AuthorizationResult = std::variant<Authorization, AuthorizationError>;

/// Asks the player to accept a Discord application, in the Discord client or in the browser.
///
/// @code
/// authorizer.authorize(applicationId, [&](const rtype::discord::AuthorizationResult &result) {
///     if (const auto *accepted = std::get_if<rtype::discord::Authorization>(&result)) {
///         lobby.discordConfirm({accepted->code, accepted->codeVerifier, accepted->redirectUri}, ...);
///     }
/// });
/// @endcode
///
/// Everything runs on the thread that polls Discord (Session::poll(), the fake in the tests): in the client,
/// the game loop.
class Authorizer
{
public:
    using Done = std::function<void(const AuthorizationResult &result)>;

    virtual ~Authorizer() = default;

    /// Asks the player to accept the application, with the scope `identify` only. `done` is called once,
    /// when Discord is polled. Another authorize() gives up the one running, whose `done` is never called.
    virtual void authorize(std::uint64_t applicationId, Done done) = 0;
    /// Gives up the authorization running, if any: its `done` is never called.
    virtual void cancel() = 0;
};

} // namespace rtype::discord
