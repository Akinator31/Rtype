#pragma once

#include "rtype/discord/Connection.hpp"

#include <memory>

namespace rtype::discord
{

class Session;

/// The Connection of the game, through the Discord Social SDK: `Client::UpdateToken` with the Bearer token
/// the master gave (DISCORD_TOKEN), then `Client::Connect`, the status followed by `SetStatusChangedCallback`
/// and the expiry by `SetTokenExpirationCallback`. On the client of the Session, whose callbacks it owns:
/// nothing else sets them. Once connected, the client of the SDK is the one of the application of the master,
/// and the activity set before is cleared by Discord. No token is logged.
class SocialSdkConnection final : public Connection
{
    struct State;
    Session &_session;
    std::shared_ptr<State> _state;

    /// Gives the client of the SDK the callbacks of the connection, once.
    void install();

public:
    /// Connects through the client of `session`, which outlives it.
    explicit SocialSdkConnection(Session &session);
    /// Disconnects, if connected.
    ~SocialSdkConnection() override;

    SocialSdkConnection(const SocialSdkConnection &) = delete;
    SocialSdkConnection &operator=(const SocialSdkConnection &) = delete;

    void connect(const std::string &accessToken) override;
    void disconnect() override;
    [[nodiscard]] ConnectionStatus status() const override;
    void onStatusChanged(StatusChanged changed) override;
    void onTokenExpiring(TokenExpiring expiring) override;
};

} // namespace rtype::discord
