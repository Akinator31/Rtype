#pragma once

// The Discord Social SDK of the tests: the interfaces of libs/discord, one fake per feature, sharing the
// poll() of a FakeSocialSdk as the implementations of the SDK share the one of a rtype::discord::Session.
// Nothing reaches Discord; what the game asks is kept, and what Discord would answer is set by the test and
// delivered at the next poll(), as the SDK runs its callbacks in RunCallbacks().

#include "rtype/discord/Authorizer.hpp"
#include "rtype/discord/Connection.hpp"
#include "rtype/discord/Presence.hpp"
#include "rtype/discord/VoiceCall.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
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
    /// The applications asked, in order, and their scopes.
    std::vector<std::uint64_t> applications;
    std::vector<std::string> scopes;

    explicit FakeAuthorizer(FakeCallbacks &callbacks) : _callbacks(callbacks) {}

    void authorize(std::uint64_t applicationId, const std::string &asked, Done done) override
    {
        cancel();
        ++_attempt;
        applications.push_back(applicationId);
        scopes.push_back(asked);
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

/// The lobby of a game and its call: the test connects it, makes it fail, puts members in it and has them
/// talk. What the game asks of the call is kept.
class FakeVoiceCall final : public rtype::discord::VoiceCall
{
    FakeCallbacks &_callbacks;
    rtype::discord::CallStatus _status = rtype::discord::CallStatus::Idle;
    /// Each start() counts: what is answered for an older one is dropped, as with the SDK.
    std::uint64_t _attempt = 0;

    void change(rtype::discord::CallStatus status)
    {
        _callbacks.post([this, status, attempt = _attempt] {
            if (attempt == _attempt && _status != rtype::discord::CallStatus::Idle) {
                _status = status;
            }
        });
    }

public:
    /// The secrets and metadata of every start(), in order.
    std::vector<std::string> secrets;
    std::vector<std::map<std::string, std::string>> metadata;
    /// Where the voices received go, as the SDK keeps it until the call ends.
    Received received;
    /// Where the microphone goes, as the SDK keeps it until the call ends.
    Captured captured;
    int ends = 0;
    std::vector<rtype::discord::CallMember> others;
    bool selfSpeaking = false;
    bool selfMuted = false;
    bool selfDeafened = false;
    rtype::discord::CallTalkMode mode = rtype::discord::CallTalkMode::PushToTalk;
    float threshold = 0.0f;
    bool pushToTalk = false;
    float inputVolume = 1.0f;
    bool noiseSuppression = false;
    bool gainControl = false;
    /// Without the connection of the token, start() fails at once, as the SDK does.
    bool discordConnected = true;

    explicit FakeVoiceCall(FakeCallbacks &callbacks) : _callbacks(callbacks) {}

    void start(const std::string &secret, const std::map<std::string, std::string> &asked, Received sink,
               Captured microphone) override
    {
        end();
        ++_attempt;
        secrets.push_back(secret);
        metadata.push_back(asked);
        if (!discordConnected) {
            _status = rtype::discord::CallStatus::Failed;
            return;
        }
        _status = rtype::discord::CallStatus::Joining;
        received = std::move(sink);
        captured = std::move(microphone);
    }

    void end() override
    {
        if (_status == rtype::discord::CallStatus::Idle) {
            return;
        }
        ++_attempt;
        ++ends;
        _status = rtype::discord::CallStatus::Idle;
        received = nullptr;
        captured = nullptr;
        others.clear();
    }

    [[nodiscard]] rtype::discord::CallStatus status() const override { return _status; }
    [[nodiscard]] std::vector<rtype::discord::CallMember> members() const override { return others; }
    [[nodiscard]] bool speaking() const override { return selfSpeaking; }
    void setSelfMute(bool muted) override { selfMuted = muted; }
    void setSelfDeaf(bool deafened) override { selfDeafened = deafened; }
    void setTalkMode(rtype::discord::CallTalkMode talkMode, float level) override
    {
        mode = talkMode;
        threshold = level;
    }
    void setPushToTalk(bool active) override { pushToTalk = active; }
    void setInputVolume(float volume) override { inputVolume = volume; }
    void setProcessing(bool noise, bool gain) override
    {
        noiseSuppression = noise;
        gainControl = gain;
    }

    /// The lobby is joined and the call connected: Connected at the next poll().
    void connect() { change(rtype::discord::CallStatus::Connected); }
    /// The lobby cannot be joined, or the call drops: Failed at the next poll().
    void fail() { change(rtype::discord::CallStatus::Failed); }
    /// A member of the lobby, connected to the call, with the id the master gave its player.
    void addMember(std::uint64_t userId, std::uint32_t playerId)
    {
        others.push_back(
            {.userId = userId, .metadata = {{"player", std::to_string(playerId)}}, .speaking = false});
    }
    /// The player talks in its microphone: its samples, as the audio thread of Discord gives them.
    void say(std::span<const std::int16_t> samples, std::int32_t rate = 48000, std::size_t channels = 1)
    {
        if (captured) {
            captured(samples, rate, channels);
        }
    }
    /// A member talks: its samples, as the audio thread of Discord gives them.
    void talk(std::uint64_t userId, std::span<const std::int16_t> samples, std::int32_t rate = 48000,
              std::size_t channels = 1)
    {
        if (received) {
            received(userId, samples, rate, channels);
        }
    }
};

/// Every feature of Discord, on one poll(), as the client builds them on one rtype::discord::Session.
struct FakeSocialSdk {
    FakeCallbacks callbacks;
    FakeAuthorizer authorizer{callbacks};
    FakePresence presence;
    FakeConnection connection{callbacks};
    FakeVoiceCall call{callbacks};

    void poll() { callbacks.poll(); }
};

} // namespace testing
