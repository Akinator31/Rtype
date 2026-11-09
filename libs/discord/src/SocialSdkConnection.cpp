#include "rtype/discord/SocialSdkConnection.hpp"

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

ConnectionStatus statusOf(discordpp::Client::Status status)
{
    switch (status) {
    case discordpp::Client::Status::Ready:
        return ConnectionStatus::Ready;
    case discordpp::Client::Status::Disconnected:
        return ConnectionStatus::Disconnected;
    default:
        return ConnectionStatus::Connecting;
    }
}

} // namespace

/// What the callbacks of the SDK reach: they may run after the connection is gone, and then do nothing.
struct SocialSdkConnection::State {
    ConnectionStatus status = ConnectionStatus::Disconnected;
    StatusChanged changed;
    TokenExpiring expiring;
    bool installed = false;

    void change(ConnectionStatus next)
    {
        if (next == status) {
            return;
        }
        status = next;
        if (changed) {
            changed(next);
        }
    }
};

SocialSdkConnection::SocialSdkConnection(Session &session)
    : _session(session), _state(std::make_shared<State>())
{
}

SocialSdkConnection::~SocialSdkConnection()
{
    disconnect();
    _state->changed = nullptr;
    _state->expiring = nullptr;
}

void SocialSdkConnection::install()
{
    if (_state->installed) {
        return;
    }
    _state->installed = true;
    discordpp::Client &client = _session.client();
    client.SetStatusChangedCallback([state = std::weak_ptr<State>(_state)](discordpp::Client::Status status,
                                                                           discordpp::Client::Error error,
                                                                           int32_t detail) {
        const std::shared_ptr<State> alive = state.lock();
        if (!alive) {
            return;
        }
        if (error != discordpp::Client::Error::None) {
            logger().warning("Discord connection: {}, {} ({})", discordpp::Client::StatusToString(status),
                             discordpp::Client::ErrorToString(error), detail);
        } else {
            logger().info("Discord connection: {}", discordpp::Client::StatusToString(status));
        }
        alive->change(statusOf(status));
    });
    client.SetTokenExpirationCallback([state = std::weak_ptr<State>(_state)] {
        const std::shared_ptr<State> alive = state.lock();
        if (alive && alive->expiring) {
            logger().info("The Discord token of the player expires");
            alive->expiring();
        }
    });
}

void SocialSdkConnection::connect(const std::string &accessToken)
{
    install();
    discordpp::Client &client = _session.client();
    const bool connecting = _state->status == ConnectionStatus::Disconnected;
    if (connecting) {
        _state->change(ConnectionStatus::Connecting);
    }
    // The next Connect() must use the token: it waits for the SDK to have it
    client.UpdateToken(discordpp::AuthorizationTokenType::Bearer, accessToken,
                       [session = &_session, state = std::weak_ptr<State>(_state),
                        connecting](const discordpp::ClientResult &result) {
                           const std::shared_ptr<State> alive = state.lock();
                           if (!alive) {
                               return;
                           }
                           if (!result.Successful()) {
                               logger().warning("Discord refused the token of the player: {}",
                                                result.Error());
                               alive->change(ConnectionStatus::Disconnected);
                               return;
                           }
                           if (connecting) {
                               session->client().Connect();
                           }
                       });
}

void SocialSdkConnection::disconnect()
{
    if (_state->status == ConnectionStatus::Disconnected) {
        return;
    }
    _session.client().Disconnect();
    // Told now: the SDK may not call back for a client it disconnects while the game closes
    _state->change(ConnectionStatus::Disconnected);
}

ConnectionStatus SocialSdkConnection::status() const
{
    return _state->status;
}

void SocialSdkConnection::onStatusChanged(StatusChanged changed)
{
    _state->changed = std::move(changed);
}

void SocialSdkConnection::onTokenExpiring(TokenExpiring expiring)
{
    _state->expiring = std::move(expiring);
}

} // namespace rtype::discord
