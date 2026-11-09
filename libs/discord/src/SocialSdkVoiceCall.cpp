#include "rtype/discord/SocialSdkVoiceCall.hpp"

#include "rtype/discord/Session.hpp"
#include "rtype/log/Logger.hpp"

#include <discordpp.h>

#include <algorithm>
#include <optional>
#include <set>
#include <unordered_map>
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

/// What the callbacks of the SDK reach: they may run after the call is gone, and then do nothing.
struct SocialSdkVoiceCall::State {
    CallStatus status = CallStatus::Idle;
    /// Each start() counts: an answer for an older one is dropped.
    std::uint64_t attempt = 0;
    std::optional<std::uint64_t> lobbyId;
    std::optional<discordpp::Call> call;
    /// The members heard talking now, the player included.
    std::set<std::uint64_t> speaking;
    bool everConnected = false;

    // What the player chose, given to each call
    bool selfMuted = true;
    bool selfDeafened = false;
    CallTalkMode mode = CallTalkMode::PushToTalk;
    float threshold = -60.0f;
    bool pushToTalk = false;
};

SocialSdkVoiceCall::SocialSdkVoiceCall(Session &session)
    : _session(session), _state(std::make_shared<State>())
{
}

SocialSdkVoiceCall::~SocialSdkVoiceCall()
{
    end();
}

void SocialSdkVoiceCall::start(const std::string &secret, const std::map<std::string, std::string> &metadata,
                               Received received, Captured captured)
{
    end();
    const std::uint64_t attempt = ++_state->attempt;
    // Never the SDK for a player who did not start it, nor a lobby without the connection of the token
    if (!_session.started() || _session.client().GetStatus() != discordpp::Client::Status::Ready) {
        logger().info("No voice through Discord: not connected to it");
        _state->status = CallStatus::Failed;
        return;
    }
    _state->status = CallStatus::Joining;
    discordpp::Client &client = _session.client();
    const std::unordered_map<std::string, std::string> member(metadata.begin(), metadata.end());
    client.CreateOrJoinLobbyWithMetadata(
        secret, {}, member,
        [this, session = &_session, state = std::weak_ptr<State>(_state), attempt,
         received = std::move(received),
         captured = std::move(captured)](const discordpp::ClientResult &result, std::uint64_t lobbyId) {
            const std::shared_ptr<State> alive = state.lock();
            if (!alive || attempt != alive->attempt) {
                // Ended meanwhile: not kept in the lobby
                if (result.Successful()) {
                    session->client().LeaveLobby(lobbyId, [](const discordpp::ClientResult &) {});
                }
                return;
            }
            if (!result.Successful()) {
                logger().warning("Cannot join the Discord lobby of the game: {}", result.Error());
                alive->status = CallStatus::Failed;
                return;
            }
            alive->lobbyId = lobbyId;
            discordpp::Client &client = session->client();
            discordpp::Call call = client.StartCallWithAudioCallbacks(
                lobbyId,
                [received](std::uint64_t userId, std::int16_t *data, std::uint64_t samplesPerChannel,
                           std::int32_t sampleRate, std::uint64_t channels, bool &outShouldMute) {
                    // Played by the game, not by Discord
                    outShouldMute = true;
                    received(userId,
                             std::span<const std::int16_t>(
                                 data, static_cast<std::size_t>(samplesPerChannel * channels)),
                             sampleRate, static_cast<std::size_t>(channels));
                },
                [captured](std::int16_t *data, std::uint64_t samplesPerChannel, std::int32_t sampleRate,
                           std::uint64_t channels) {
                    // The microphone, for the players on the voice of the game
                    if (captured) {
                        captured(std::span<const std::int16_t>(
                                     data, static_cast<std::size_t>(samplesPerChannel * channels)),
                                 sampleRate, static_cast<std::size_t>(channels));
                    }
                });
            if (!call) {
                // Already in it
                call = client.GetCall(lobbyId);
            }
            if (!call) {
                logger().warning("Cannot start the call of the Discord lobby {}", lobbyId);
                alive->status = CallStatus::Failed;
                return;
            }
            call.SetStatusChangedCallback(
                [state](discordpp::Call::Status status, discordpp::Call::Error error, std::int32_t detail) {
                    const std::shared_ptr<State> current = state.lock();
                    if (!current || !current->call) {
                        return;
                    }
                    if (error != discordpp::Call::Error::None) {
                        logger().warning("Discord call: {}, {} ({})", discordpp::Call::StatusToString(status),
                                         discordpp::Call::ErrorToString(error), detail);
                        current->status = CallStatus::Failed;
                        return;
                    }
                    logger().info("Discord call: {}", discordpp::Call::StatusToString(status));
                    if (status == discordpp::Call::Status::Connected) {
                        current->everConnected = true;
                        current->status = CallStatus::Connected;
                    } else if (status == discordpp::Call::Status::Disconnected && current->everConnected) {
                        current->status = CallStatus::Failed;
                    }
                });
            call.SetSpeakingStatusChangedCallback([state](std::uint64_t userId, bool playing) {
                if (const std::shared_ptr<State> current = state.lock()) {
                    if (playing) {
                        current->speaking.insert(userId);
                    } else {
                        current->speaking.erase(userId);
                    }
                }
            });
            alive->call = call;
            apply();
        });
}

void SocialSdkVoiceCall::apply()
{
    if (!_state->call) {
        return;
    }
    discordpp::Call &call = *_state->call;
    call.SetSelfMute(_state->selfMuted);
    call.SetSelfDeaf(_state->selfDeafened);
    if (_state->mode == CallTalkMode::VoiceActivity) {
        call.SetAudioMode(discordpp::AudioModeType::MODE_VAD);
        call.SetVADThreshold(false, std::clamp(_state->threshold, -100.0f, 0.0f));
    } else {
        call.SetAudioMode(discordpp::AudioModeType::MODE_PTT);
        // The game keeps the key held for its release delay already
        call.SetPTTReleaseDelay(0);
        call.SetPTTActive(_state->pushToTalk);
    }
}

void SocialSdkVoiceCall::end()
{
    ++_state->attempt;
    if (_state->lobbyId) {
        discordpp::Client &client = _session.client();
        const std::uint64_t lobbyId = *_state->lobbyId;
        client.EndCall(lobbyId, [] {});
        client.LeaveLobby(lobbyId, [](const discordpp::ClientResult &result) {
            if (!result.Successful()) {
                logger().info("Cannot leave the Discord lobby of the game: {}", result.Error());
            }
        });
    }
    _state->lobbyId.reset();
    _state->call.reset();
    _state->speaking.clear();
    _state->everConnected = false;
    _state->status = CallStatus::Idle;
}

CallStatus SocialSdkVoiceCall::status() const
{
    return _state->status;
}

std::vector<CallMember> SocialSdkVoiceCall::members() const
{
    std::vector<CallMember> result;
    if (!_state->lobbyId || !_session.started()) {
        return result;
    }
    discordpp::Client &client = _session.client();
    const std::optional<discordpp::LobbyHandle> lobby = client.GetLobbyHandle(*_state->lobbyId);
    if (!lobby) {
        return result;
    }
    const std::optional<discordpp::UserHandle> self = client.GetCurrentUserV2();
    for (const discordpp::LobbyMemberHandle &member : lobby->LobbyMembers()) {
        if (!member.Connected() || (self && member.Id() == self->Id())) {
            continue;
        }
        const std::unordered_map<std::string, std::string> metadata = member.Metadata();
        result.push_back({.userId = member.Id(),
                          .metadata = {metadata.begin(), metadata.end()},
                          .speaking = _state->speaking.contains(member.Id())});
    }
    return result;
}

bool SocialSdkVoiceCall::speaking() const
{
    if (!_state->call || !_session.started()) {
        return false;
    }
    const std::optional<discordpp::UserHandle> self = _session.client().GetCurrentUserV2();
    return self && _state->speaking.contains(self->Id());
}

void SocialSdkVoiceCall::setSelfMute(bool muted)
{
    _state->selfMuted = muted;
    if (_state->call) {
        _state->call->SetSelfMute(muted);
    }
}

void SocialSdkVoiceCall::setSelfDeaf(bool deafened)
{
    _state->selfDeafened = deafened;
    if (_state->call) {
        _state->call->SetSelfDeaf(deafened);
    }
}

void SocialSdkVoiceCall::setTalkMode(CallTalkMode mode, float threshold)
{
    _state->mode = mode;
    _state->threshold = threshold;
    apply();
}

void SocialSdkVoiceCall::setPushToTalk(bool active)
{
    _state->pushToTalk = active;
    if (_state->call && _state->mode == CallTalkMode::PushToTalk) {
        _state->call->SetPTTActive(active);
    }
}

void SocialSdkVoiceCall::setInputVolume(float volume)
{
    if (_session.started()) {
        // Discord takes a percentage
        _session.client().SetInputVolume(std::clamp(volume, 0.0f, 1.0f) * 100.0f);
    }
}

void SocialSdkVoiceCall::setProcessing(bool noiseSuppression, bool gainControl)
{
    if (_session.started()) {
        _session.client().SetNoiseSuppression(noiseSuppression);
        _session.client().SetAutomaticGainControl(gainControl);
    }
}

} // namespace rtype::discord
