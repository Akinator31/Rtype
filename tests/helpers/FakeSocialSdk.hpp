#pragma once

// The Discord Social SDK of the tests: the interfaces of libs/discord, one fake per feature, sharing the
// poll() of a FakeSocialSdk as the implementations of the SDK share the one of a rtype::discord::Session.
// Nothing reaches Discord; what the game asks is kept, and what Discord would answer is set by the test and
// delivered at the next poll(), as the SDK runs its callbacks in RunCallbacks().

#include "rtype/discord/Authorizer.hpp"
#include "rtype/discord/Connection.hpp"
#include "rtype/discord/Presence.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace testing
{

/// What came back from Discord, run at the next poll().
class FakeCallbacks
{
    std::vector<std::function<void()>> _pending;

public:
    void post(std::function<void()> callback) { _pending.push_back(std::move(callback)); }

    /// Runs what was posted before, in order; what they post runs at the next poll().
    void poll()
    {
        std::vector<std::function<void()>> ready;
        ready.swap(_pending);
        for (std::function<void()> &callback : ready) {
            callback();
        }
    }
};

/// The player asked by Discord: the test answers for them with accept() or refuse().
class FakeAuthorizer final : public rtype::discord::Authorizer
{
    FakeCallbacks &_callbacks;
    /// The authorization running counts: what is answered for an older one is dropped, as with the SDK.
    std::uint64_t _attempt = 0;
    Done _done;

    void answer(rtype::discord::AuthorizationResult result)
    {
        if (!_done) {
            return;
        }
        _callbacks.post([this, attempt = _attempt, result = std::move(result)] {
            if (attempt != _attempt || !_done) {
                return;
            }
            Done done = std::move(_done);
            _done = nullptr;
            done(result);
        });
    }

public:
    /// The applications asked, in order.
    std::vector<std::uint64_t> applications;

    explicit FakeAuthorizer(FakeCallbacks &callbacks) : _callbacks(callbacks) {}

    void authorize(std::uint64_t applicationId, Done done) override
    {
        cancel();
        ++_attempt;
        applications.push_back(applicationId);
        _done = std::move(done);
    }

    void cancel() override { _done = nullptr; }

    /// Whether an authorization runs: asked and neither answered nor cancelled.
    [[nodiscard]] bool running() const { return static_cast<bool>(_done); }

    /// The player accepts the authorization running: its `done` gets `authorization` at the next poll().
    void accept(rtype::discord::Authorization authorization) { answer(std::move(authorization)); }
    /// The player refuses, or Discord cannot be reached: its `done` gets `error` at the next poll().
    void refuse(rtype::discord::AuthorizationError error) { answer(error); }
};

/// The profile of the player in Discord: what it shows.
class FakePresence final : public rtype::discord::Presence
{
public:
    /// What the profile shows, nothing once cleared.
    std::optional<rtype::discord::Activity> shown;
    /// Every update(), in order.
    std::vector<rtype::discord::Activity> updates;

    void update(const rtype::discord::Activity &activity) override
    {
        shown = activity;
        updates.push_back(activity);
    }

    void clear() override { shown.reset(); }
};

/// The connection to Discord: the test makes it ready, loses it, or lets its token expire.
class FakeConnection final : public rtype::discord::Connection
{
    FakeCallbacks &_callbacks;
    rtype::discord::ConnectionStatus _status = rtype::discord::ConnectionStatus::Disconnected;
    StatusChanged _changed;
    TokenExpiring _expiring;

    void change(rtype::discord::ConnectionStatus status)
    {
        _callbacks.post([this, status] {
            if (_status == status) {
                return;
            }
            _status = status;
            if (_changed) {
                _changed(status);
            }
        });
    }

public:
    /// The tokens given to connect(), in order.
    std::vector<std::string> tokens;

    explicit FakeConnection(FakeCallbacks &callbacks) : _callbacks(callbacks) {}

    void connect(const std::string &accessToken) override
    {
        tokens.push_back(accessToken);
        if (_status == rtype::discord::ConnectionStatus::Disconnected) {
            change(rtype::discord::ConnectionStatus::Connecting);
        }
    }

    void disconnect() override { change(rtype::discord::ConnectionStatus::Disconnected); }

    [[nodiscard]] rtype::discord::ConnectionStatus status() const override { return _status; }

    void onStatusChanged(StatusChanged changed) override { _changed = std::move(changed); }

    void onTokenExpiring(TokenExpiring expiring) override { _expiring = std::move(expiring); }

    /// Discord accepts the token: Ready at the next poll().
    void becomeReady() { change(rtype::discord::ConnectionStatus::Ready); }
    /// The connection drops: Disconnected at the next poll().
    void lose() { change(rtype::discord::ConnectionStatus::Disconnected); }
    /// The token is about to expire: the callback runs at the next poll().
    void expireToken()
    {
        _callbacks.post([this] {
            if (_expiring) {
                _expiring();
            }
        });
    }
};

/// Every feature of Discord, on one poll(), as the client builds them on one rtype::discord::Session.
struct FakeSocialSdk {
    FakeCallbacks callbacks;
    FakeAuthorizer authorizer{callbacks};
    FakePresence presence;
    FakeConnection connection{callbacks};

    void poll() { callbacks.poll(); }
};

} // namespace testing
