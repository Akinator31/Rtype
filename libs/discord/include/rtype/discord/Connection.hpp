#pragma once

#include <functional>
#include <string>

/// The connection of the client to Discord with the token of the player
/// (docs/architecture/client-online.md#discord), which lobbies, voice, relationships and invitations need.
/// The client and its tests know only this interface; its implementation on the Social SDK comes with the
/// token of Discord in the client.
namespace rtype::discord
{

/// Where the connection is.
enum class ConnectionStatus {
    /// Never connected, signed out, or lost.
    Disconnected,
    /// connect() was called, Discord is not ready yet.
    Connecting,
    /// The features that need a token can be used.
    Ready,
};

/// Connects the client to Discord with an access token of the player.
///
/// @code
/// connection.onStatusChanged([&](rtype::discord::ConnectionStatus status) {
///     if (status == rtype::discord::ConnectionStatus::Ready) { /* the lobby of the game */ }
/// });
/// connection.onTokenExpiring([&] { /* DISCORD_REFRESH to the master */ });
/// connection.connect(accessToken);
/// @endcode
///
/// Everything runs on the thread that polls Discord: in the client, the game loop. Without Discord, the
/// status stays Disconnected: the features that need it are not available, never an error.
class Connection
{
public:
    using StatusChanged = std::function<void(ConnectionStatus status)>;
    using TokenExpiring = std::function<void()>;

    virtual ~Connection() = default;

    /// Connects with `accessToken` (a Bearer token, secret: never logged), or gives a new one to a connection
    /// whose token expires.
    virtual void connect(const std::string &accessToken) = 0;
    /// Disconnects: at the sign-out, or the deletion of the account.
    virtual void disconnect() = 0;
    [[nodiscard]] virtual ConnectionStatus status() const = 0;
    /// Called at every change of status, when Discord is polled.
    virtual void onStatusChanged(StatusChanged changed) = 0;
    /// Called when the token is about to expire: the client asks the master for a new one.
    virtual void onTokenExpiring(TokenExpiring expiring) = 0;
};

} // namespace rtype::discord
