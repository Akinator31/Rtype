#pragma once

#include <memory>

namespace discordpp
{
class Client;
} // namespace discordpp

namespace rtype::discord
{

/// The one `discordpp::Client` of the client (docs/architecture/client-online.md#discord), shared by every
/// feature of libs/discord built on the Discord Social SDK: the sign-in (SocialSdkAuthorizer), then the
/// presence, the connection and the voice. Created at the first client() only: a player who never uses
/// Discord never starts the SDK. Its logs go to the logger `discord`, warnings and errors only.
///
/// @code
/// rtype::discord::Session session;
/// rtype::discord::SocialSdkAuthorizer authorizer(session);
/// // In the game loop, every frame
/// session.poll();
/// @endcode
///
/// Everything runs on the thread that calls poll(): in the client, the game loop. The features that use the
/// session are destroyed before it.
class Session
{
    struct State;
    std::unique_ptr<State> _state;

public:
    Session();
    ~Session();

    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    /// Whether the SDK started: a feature asked for the client.
    [[nodiscard]] bool started() const;
    /// The client of the SDK, created at the first call. For the implementations of libs/discord only: the
    /// rest of the game knows their interfaces.
    [[nodiscard]] discordpp::Client &client();
    /// Runs what came back from Discord (`discordpp::RunCallbacks`), once the SDK started.
    void poll();
};

} // namespace rtype::discord
